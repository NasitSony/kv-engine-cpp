#include "kv/raft_transport.h"

namespace kv {

void InProcessTransport::Register(RaftNode* n) {
  std::lock_guard<std::mutex> g(mu_);
  nodes_[n->id()] = n;
}

RequestVoteResp InProcessTransport::RequestVote(int peer_id, const RequestVoteReq& req) {
  std::lock_guard<std::mutex> g(mu_);
  return nodes_.at(peer_id)->OnRequestVote(req);
}

AppendEntriesResp InProcessTransport::AppendEntries(int peer_id, const AppendEntriesReq& req) {
  std::lock_guard<std::mutex> g(mu_);
  return nodes_.at(peer_id)->OnAppendEntries(req);
}

} // namespace kv