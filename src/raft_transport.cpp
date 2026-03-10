#include "kv/raft_transport.h"

namespace kv {

void InProcessTransport::Register(RaftNode* n) {
  std::lock_guard<std::mutex> g(mu_);
  nodes_[n->id()] = n;
}

void InProcessTransport::Unregister(int id) {
  std::lock_guard<std::mutex> g(mu_);
  nodes_.erase(id);
}

RequestVoteResp InProcessTransport::RequestVote(int peer_id, const RequestVoteReq& req) {
  std::lock_guard<std::mutex> g(mu_);

  auto it = nodes_.find(peer_id);
  if (it == nodes_.end()) {
    RequestVoteResp resp;
    resp.term = req.term;
    resp.vote_granted = false;
    return resp; // peer unavailable
  }

  return it->second->OnRequestVote(req);
}

AppendEntriesResp InProcessTransport::AppendEntries(int peer_id, const AppendEntriesReq& req) {
  std::lock_guard<std::mutex> g(mu_);

  auto it = nodes_.find(peer_id);
  if (it == nodes_.end()) {
    AppendEntriesResp resp;
    resp.term = req.term;
    resp.success = false;
    return resp; // peer unavailable
  }

  return it->second->OnAppendEntries(req);
}

} // namespace kv