#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <optional>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "kv/raft_sm.h"

namespace kv {

enum class RaftRole { Follower, Candidate, Leader };
enum class OpType : uint8_t { Put = 1, Del = 2 };

struct LogEntry {
  uint64_t term = 0;
  OpType op = OpType::Put;
  std::string key;
  std::string value; // empty for Del
};

struct RequestVoteReq {
  uint64_t term = 0;
  int candidate_id = -1;
  uint64_t last_log_index = 0;
  uint64_t last_log_term = 0;
};

struct RequestVoteResp {
  uint64_t term = 0;
  bool vote_granted = false;
};

struct AppendEntriesReq {
  uint64_t term = 0;
  int leader_id = -1;

  uint64_t prev_log_index = 0; // 0 means "before first entry"
  uint64_t prev_log_term = 0;

  std::vector<LogEntry> entries; // empty = heartbeat
  uint64_t leader_commit = 0;
};

struct AppendEntriesResp {
  uint64_t term = 0;
  bool success = false;
};

class IRaftTransport {
 public:
  virtual ~IRaftTransport() = default;
  virtual RequestVoteResp RequestVote(int peer_id, const RequestVoteReq& req) = 0;
  virtual AppendEntriesResp AppendEntries(int peer_id, const AppendEntriesReq& req) = 0;
};

class RaftNode {
 public:
  RaftNode(int id, std::vector<int> peers, IRaftTransport* transport, IRaftStateMachine* sm);
  ~RaftNode();

  void Start();
  void Stop();

  // Client API (leader only)
  bool ProposePut(std::string key, std::string value);
  bool ProposeDel(std::string key);

  // RPC handlers
  RequestVoteResp OnRequestVote(const RequestVoteReq& req);
  AppendEntriesResp OnAppendEntries(const AppendEntriesReq& req);

  // Observability
  int id() const { return id_; }
  RaftRole role() const { return role_; }
  uint64_t term() const { return current_term_; }
  std::optional<int> leader_id() const { return leader_id_; }

 private:
  // background loops
  void ElectionLoop();
  void HeartbeatLoop();
  void ApplyLoop();

  void BecomeFollowerLocked(uint64_t new_term, std::optional<int> leader);
  void BecomeCandidateLocked();
  void BecomeLeaderLocked();

  void ResetElectionTimeoutLocked();
  bool IsUpToDateLocked(uint64_t cand_last_idx, uint64_t cand_last_term) const;

  // Log helpers: Raft index is 1..N, stored at log_[index-1]
  uint64_t LastLogIndexLocked() const { return static_cast<uint64_t>(log_.size()); }
  uint64_t LastLogTermLocked() const { return log_.empty() ? 0 : log_.back().term; }

  uint64_t TermAtLocked(uint64_t idx) const {
    if (idx == 0) return 0;
    if (idx > log_.size()) return 0;
    return log_[static_cast<size_t>(idx - 1)].term;
  }

  bool IsLeaderLocked() const { return role_ == RaftRole::Leader; }

 private:
  const int id_;
  const std::vector<int> peers_;
  IRaftTransport* const transport_;
  IRaftStateMachine* const sm_;

  mutable std::mutex mu_;
  std::atomic<bool> running_{false};

  // "Persistent-ish" (in-memory for now)
  uint64_t current_term_ = 0;
  std::optional<int> voted_for_;
  std::vector<LogEntry> log_;

  // Volatile
  RaftRole role_ = RaftRole::Follower;
  std::optional<int> leader_id_;

  uint64_t commit_index_ = 0; // highest committed log index
  uint64_t last_applied_ = 0; // highest applied log index

  // Timers
  std::chrono::steady_clock::time_point last_heard_{};
  std::chrono::milliseconds election_timeout_{200};
  std::mt19937 rng_;

  // Threads
  std::thread election_thread_;
  std::thread heartbeat_thread_;
  std::thread apply_thread_;
};

} // namespace kv