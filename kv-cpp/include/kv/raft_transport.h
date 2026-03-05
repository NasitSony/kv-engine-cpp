#pragma once
#include "kv/raft.h"
#include <mutex>
#include <unordered_map>

namespace kv {

// Simple in-process transport: directly calls peer handler methods.
// You can later swap this with TCP without changing RaftNode.
class InProcessTransport : public IRaftTransport {
 public:
  void Register(RaftNode* node);

  RequestVoteResp RequestVote(int peer_id, const RequestVoteReq& req) override;
  AppendEntriesResp AppendEntries(int peer_id, const AppendEntriesReq& req) override;

 private:
  std::mutex mu_;
  std::unordered_map<int, RaftNode*> nodes_;
};

} // namespace kv