#include "kv/raft.h"

#include <iostream>
#include <algorithm>

namespace kv {

static constexpr auto kHeartbeatInterval = std::chrono::milliseconds(50);
static constexpr auto kApplyPoll = std::chrono::milliseconds(10);
static constexpr int kMinElectionMs = 150;
static constexpr int kMaxElectionMs = 300;

RaftNode::RaftNode(int id, std::vector<int> peers, IRaftTransport* transport, IRaftStateMachine* sm)
    : id_(id),
      peers_(std::move(peers)),
      transport_(transport),
      sm_(sm),
      rng_(static_cast<uint32_t>(id * 9973 + 17)) {}

RaftNode::~RaftNode() { Stop(); }

void RaftNode::Start() {
  bool expected = false;
  if (!running_.compare_exchange_strong(expected, true)) return;

  {
    std::lock_guard<std::mutex> g(mu_);
    last_heard_ = std::chrono::steady_clock::now();
    ResetElectionTimeoutLocked();
  }

  election_thread_ = std::thread([this] { ElectionLoop(); });
  heartbeat_thread_ = std::thread([this] { HeartbeatLoop(); });
  apply_thread_ = std::thread([this] { ApplyLoop(); });
}

void RaftNode::Stop() {
  if (!running_.exchange(false)) return;

  if (election_thread_.joinable()) election_thread_.join();
  if (heartbeat_thread_.joinable()) heartbeat_thread_.join();
  if (apply_thread_.joinable()) apply_thread_.join();
}

void RaftNode::ResetElectionTimeoutLocked() {
  std::uniform_int_distribution<int> dist(kMinElectionMs, kMaxElectionMs);
  election_timeout_ = std::chrono::milliseconds(dist(rng_));
}

bool RaftNode::IsUpToDateLocked(uint64_t cand_last_idx, uint64_t cand_last_term) const {
  uint64_t my_term = LastLogTermLocked();
  uint64_t my_idx = LastLogIndexLocked();
  if (cand_last_term != my_term) return cand_last_term > my_term;
  return cand_last_idx >= my_idx;
}

void RaftNode::AdvanceCommitIndexLocked() {
  std::vector<uint64_t> match_values;
  match_values.push_back(LastLogIndexLocked()); // leader itself

  for (const auto& kv : match_index_) {
    match_values.push_back(kv.second);
  }

  std::sort(match_values.begin(), match_values.end());

  // majority position
  uint64_t N = match_values[match_values.size() / 2];

  if (N > commit_index_) {
    // Raft rule: only commit entries from current term
    if (TermAtLocked(N) == current_term_) {
      commit_index_ = N;
      std::cerr << "[raft] commit_index -> " << commit_index_ << "\n";
    }
  }
}

void RaftNode::BecomeFollowerLocked(uint64_t new_term, std::optional<int> leader) {
  role_ = RaftRole::Follower;
  leader_id_ = leader;
  current_term_ = new_term;
  voted_for_.reset();
  last_heard_ = std::chrono::steady_clock::now();
  ResetElectionTimeoutLocked();
}

void RaftNode::BecomeCandidateLocked() {
  role_ = RaftRole::Candidate;
  leader_id_.reset();
  current_term_ += 1;
  voted_for_ = id_;
  last_heard_ = std::chrono::steady_clock::now();
  ResetElectionTimeoutLocked();
}

void RaftNode::BecomeLeaderLocked() {
  role_ = RaftRole::Leader;
  leader_id_ = id_;
  std::cerr << "[raft] node " << id_ << " became LEADER term=" << current_term_ << "\n";

  uint64_t last = LastLogIndexLocked();
  for (int p : peers_) {
    next_index_[p] = last + 1;   // start sending from end
    match_index_[p] = 0;         // nothing replicated yet
  }
  std::cerr << "[raft] init next_index for " << peers_.size() << " peers\n";
}


AppendEntriesReq RaftNode::BuildAppendEntriesLocked(int peer_id) const {
  AppendEntriesReq req;
  req.term = current_term_;
  req.leader_id = id_;
  req.leader_commit = commit_index_;

  uint64_t next = 1;
  auto it = next_index_.find(peer_id);
  if (it != next_index_.end()) {
    next = it->second;
  }

  req.prev_log_index = (next == 0 || next == 1) ? 0 : (next - 1);
  req.prev_log_term = TermAtLocked(req.prev_log_index);

  // send suffix [next .. end]
  if (next >= 1 && next <= log_.size()) {
    for (size_t i = static_cast<size_t>(next - 1); i < log_.size(); ++i) {
      req.entries.push_back(log_[i]);
    }
  }

  return req;
}

// ---------------- RPC handlers ----------------

RequestVoteResp RaftNode::OnRequestVote(const RequestVoteReq& req) {
  std::lock_guard<std::mutex> g(mu_);

  RequestVoteResp resp;
  resp.term = current_term_;
  resp.vote_granted = false;

  if (req.term < current_term_) return resp;

  if (req.term > current_term_) {
    BecomeFollowerLocked(req.term, std::nullopt);
  }

  resp.term = current_term_;

  bool can_vote = !voted_for_.has_value() || voted_for_.value() == req.candidate_id;
  if (can_vote && IsUpToDateLocked(req.last_log_index, req.last_log_term)) {
    voted_for_ = req.candidate_id;
    last_heard_ = std::chrono::steady_clock::now();
    ResetElectionTimeoutLocked();
    resp.vote_granted = true;
  }

  return resp;
}

AppendEntriesResp RaftNode::OnAppendEntries(const AppendEntriesReq& req) {
  std::lock_guard<std::mutex> g(mu_);

  AppendEntriesResp resp;
  resp.term = current_term_;
  resp.success = false;

  if (req.term < current_term_) return resp;

  if (req.term > current_term_) {
    BecomeFollowerLocked(req.term, req.leader_id);
  } else {
    if (role_ != RaftRole::Follower) role_ = RaftRole::Follower;
    leader_id_ = req.leader_id;
    last_heard_ = std::chrono::steady_clock::now();
    ResetElectionTimeoutLocked();
  }

  // 1) Consistency check: do we have prev_log_index with matching term?
  if (req.prev_log_index > LastLogIndexLocked()) {
    resp.term = current_term_;
    resp.success = false;
    return resp;
  }
  if (TermAtLocked(req.prev_log_index) != req.prev_log_term) {
    resp.term = current_term_;
    resp.success = false;
    return resp;
  }

  // 2) Append entries: truncate any conflicting suffix (minimal)
  if (!req.entries.empty()) {
    // Keep prefix up to prev_log_index
    if (req.prev_log_index < LastLogIndexLocked()) {
      log_.resize(static_cast<size_t>(req.prev_log_index));
    }
    // Append new entries
    for (const auto& e : req.entries) log_.push_back(e);
  }

  // 3) Update commit index from leader
  if (req.leader_commit > commit_index_) {
    uint64_t last_new = LastLogIndexLocked();
    commit_index_ = std::min(req.leader_commit, last_new);
  }

  resp.term = current_term_;
  resp.success = true;
  return resp;
}

// ---------------- Client proposals (leader) ----------------

bool RaftNode::ProposePut(std::string key, std::string value) {
  // Build entry
  LogEntry e;
  {
    std::lock_guard<std::mutex> g(mu_);
    if (!IsLeaderLocked()) return false;
    e.term = current_term_;
    e.op = OpType::Put;
    e.key = std::move(key);
    e.value = std::move(value);

    log_.push_back(e);
  }

  // Replicate (synchronously, minimal)
  int acks = 1; // self
  int total = 1 + static_cast<int>(peers_.size());
  int needed = total / 2 + 1;

  uint64_t my_term = 0;
  uint64_t my_index = 0;
  uint64_t prev_idx = 0;
  uint64_t prev_term = 0;

  {
    std::lock_guard<std::mutex> g(mu_);
    my_term = current_term_;
    my_index = LastLogIndexLocked();
    prev_idx = my_index - 1;
    prev_term = TermAtLocked(prev_idx);
  }

  AppendEntriesReq req;
  req.term = my_term;
  req.leader_id = id_;
  req.prev_log_index = prev_idx;
  req.prev_log_term = prev_term;
  req.entries = {e};           // single entry
  req.leader_commit = 0;       // commit sent after majority

  for (int p : peers_) {
    AppendEntriesResp resp = transport_->AppendEntries(p, req);

    std::lock_guard<std::mutex> g(mu_);
    if (!IsLeaderLocked()) return false;
    if (resp.term > current_term_) {
      BecomeFollowerLocked(resp.term, std::nullopt);
      return false;
    }
    if (resp.success) acks++;
    if (acks >= needed) break;
  }

  if (acks < needed) return false;

  
  // Send commit index via heartbeat
  AppendEntriesReq hb;
  {
    std::lock_guard<std::mutex> g(mu_);
    hb.term = current_term_;
    hb.leader_id = id_;
    hb.prev_log_index = LastLogIndexLocked();
    hb.prev_log_term = TermAtLocked(hb.prev_log_index);
    hb.leader_commit = commit_index_;
  }

  for (int p : peers_) {
    AppendEntriesResp resp = transport_->AppendEntries(p, hb);
    std::lock_guard<std::mutex> g(mu_);
    if (resp.term > current_term_) {
      BecomeFollowerLocked(resp.term, std::nullopt);
      break;
    }
  }

  return true;
}

bool RaftNode::ProposeDel(std::string key) {
  LogEntry e;
  {
    std::lock_guard<std::mutex> g(mu_);
    if (!IsLeaderLocked()) return false;
    e.term = current_term_;
    e.op = OpType::Del;
    e.key = std::move(key);
    e.value.clear();
    log_.push_back(e);
  }

  int acks = 1;
  int total = 1 + static_cast<int>(peers_.size());
  int needed = total / 2 + 1;

  uint64_t my_term = 0;
  uint64_t my_index = 0;
  uint64_t prev_idx = 0;
  uint64_t prev_term = 0;

  {
    std::lock_guard<std::mutex> g(mu_);
    my_term = current_term_;
    my_index = LastLogIndexLocked();
    prev_idx = my_index - 1;
    prev_term = TermAtLocked(prev_idx);
  }

  AppendEntriesReq req;
  req.term = my_term;
  req.leader_id = id_;
  req.prev_log_index = prev_idx;
  req.prev_log_term = prev_term;
  req.entries = {e};
  req.leader_commit = 0;

  for (int p : peers_) {
    AppendEntriesResp resp = transport_->AppendEntries(p, req);

    std::lock_guard<std::mutex> g(mu_);
    if (!IsLeaderLocked()) return false;
    if (resp.term > current_term_) {
      BecomeFollowerLocked(resp.term, std::nullopt);
      return false;
    }
    if (resp.success) acks++;
    if (acks >= needed) break;
  }

  if (acks < needed) return false;



  AppendEntriesReq hb;
  {
    std::lock_guard<std::mutex> g(mu_);
    hb.term = current_term_;
    hb.leader_id = id_;
    hb.prev_log_index = LastLogIndexLocked();
    hb.prev_log_term = TermAtLocked(hb.prev_log_index);
    hb.leader_commit = commit_index_;
  }
  for (int p : peers_) {
    AppendEntriesResp resp = transport_->AppendEntries(p, hb);
    std::lock_guard<std::mutex> g(mu_);
    if (resp.term > current_term_) {
      BecomeFollowerLocked(resp.term, std::nullopt);
      break;
    }
  }

  return true;
}

// ---------------- Background loops ----------------

void RaftNode::ElectionLoop() {
  while (running_.load()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    uint64_t my_term = 0;
    RaftRole my_role;
    std::chrono::steady_clock::time_point last;
    std::chrono::milliseconds timeout(200);

    {
      std::lock_guard<std::mutex> g(mu_);
      my_term = current_term_;
      my_role = role_;
      last = last_heard_;
      timeout = election_timeout_;
    }

    if (my_role == RaftRole::Leader) continue;

    auto now = std::chrono::steady_clock::now();
    if (now - last < timeout) continue;

    uint64_t term_started = 0;
    uint64_t last_idx = 0;
    uint64_t last_term = 0;

    {
      std::lock_guard<std::mutex> g(mu_);
      BecomeCandidateLocked();
      term_started = current_term_;
      last_idx = LastLogIndexLocked();
      last_term = LastLogTermLocked();
    }

    int votes = 1;
    int total = 1 + static_cast<int>(peers_.size());
    int needed = total / 2 + 1;

    RequestVoteReq r;
    r.term = term_started;
    r.candidate_id = id_;
    r.last_log_index = last_idx;
    r.last_log_term = last_term;

    for (int p : peers_) {
      RequestVoteResp resp = transport_->RequestVote(p, r);

      std::lock_guard<std::mutex> g(mu_);

      if (role_ != RaftRole::Candidate) break;
      if (current_term_ != term_started) break;

      if (resp.term > current_term_) {
        BecomeFollowerLocked(resp.term, std::nullopt);
        break;
      }

      if (resp.vote_granted) votes++;

      if (votes >= needed) {
        BecomeLeaderLocked();
        break;
      }
    }
  }
}

void RaftNode::HeartbeatLoop() {
  while (running_.load()) {
    std::this_thread::sleep_for(kHeartbeatInterval);

    std::vector<int> peers_copy;

    {
      std::lock_guard<std::mutex> g(mu_);
      if (role_ != RaftRole::Leader) continue;
      peers_copy = peers_;
    }

    for (int p : peers_copy) {
      AppendEntriesReq req;
      {
        std::lock_guard<std::mutex> g(mu_);
        if (role_ != RaftRole::Leader) break;
        req = BuildAppendEntriesLocked(p);
      }

      AppendEntriesResp resp = transport_->AppendEntries(p, req);

      std::lock_guard<std::mutex> g(mu_);
      if (resp.term > current_term_) {
        BecomeFollowerLocked(resp.term, std::nullopt);
        break;
      }

      // For now: only handle success path
      if (resp.success) {
        if (!req.entries.empty()) {
          uint64_t sent_last = req.prev_log_index + req.entries.size();
          match_index_[p] = sent_last;
          next_index_[p] = sent_last + 1;
        }
        AdvanceCommitIndexLocked();
        std::cerr << "[raft] peer " << p
          << " match=" << match_index_[p]
          << " next=" << next_index_[p] << "\n";
      }else {
        // ❌ Follower rejected AppendEntries
        // Move nextIndex back and retry later
        auto it = next_index_.find(p);
        if (it != next_index_.end() && it->second > 1) {
           //it->second -= 1;
           it->second = std::max<uint64_t>(1, it->second - 1);
        }
             std::cerr << "[raft] backtrack peer " << p
              << " next=" << next_index_[p] << "\n";
}
    }
  }
}
void RaftNode::ApplyLoop() {
  while (running_.load()) {
    std::this_thread::sleep_for(kApplyPoll);

    LogEntry to_apply;
    bool have = false;

    {
      std::lock_guard<std::mutex> g(mu_);
      if (last_applied_ < commit_index_) {
        uint64_t idx = last_applied_ + 1; // 1..N
        to_apply = log_[static_cast<size_t>(idx - 1)];
        last_applied_ = idx;
        have = true;
      }
    }

    if (!have) continue;

    // Apply outside lock
    if (to_apply.op == OpType::Put) {
      sm_->ApplyPut(std::move(to_apply.key), std::move(to_apply.value));
    } else {
      sm_->ApplyDel(to_apply.key);
    }
  }
}

} // namespace kv