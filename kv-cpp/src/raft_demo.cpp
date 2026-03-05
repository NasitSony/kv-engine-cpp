#include "kv/raft_transport.h"
#include "kv/kv_store.h"
#include <chrono>
#include <iostream>
#include <thread>

static const char* role_name(kv::RaftRole r) {
  switch (r) {
    case kv::RaftRole::Follower: return "F";
    case kv::RaftRole::Candidate: return "C";
    case kv::RaftRole::Leader: return "L";
  }
  return "?";
}

int main() {
  kv::InProcessTransport t;

  kv::KVStore s1, s2, s3;
  kv::RaftNode n1(1, {2,3}, &t, &s1);
  kv::RaftNode n2(2, {1,3}, &t, &s2);
  kv::RaftNode n3(3, {1,2}, &t, &s3);

  t.Register(&n1); t.Register(&n2); t.Register(&n3);

  n1.Start(); n2.Start(); n3.Start();

  // Wait for leader
  kv::RaftNode* leader = nullptr;
  for (int i = 0; i < 40 && !leader; i++) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    if (n1.role() == kv::RaftRole::Leader) leader = &n1;
    else if (n2.role() == kv::RaftRole::Leader) leader = &n2;
    else if (n3.role() == kv::RaftRole::Leader) leader = &n3;

    std::cout << "n1(" << role_name(n1.role()) << ",t=" << n1.term() << ") "
              << "n2(" << role_name(n2.role()) << ",t=" << n2.term() << ") "
              << "n3(" << role_name(n3.role()) << ",t=" << n3.term() << ")\n";
  }

  if (!leader) {
    std::cerr << "No leader elected\n";
    n1.Stop(); n2.Stop(); n3.Stop();
    return 1;
  }

  std::cout << "Leader is node " << leader->id() << "\n";

  // Propose a write
  bool ok = leader->ProposePut("a", "100");
  std::cout << "ProposePut(a=100) -> " << (ok ? "true" : "false") << "\n";

  // Give time to apply on all nodes
  std::this_thread::sleep_for(std::chrono::milliseconds(300));

  auto v1 = s1.get("a");
  auto v2 = s2.get("a");
  auto v3 = s3.get("a");

  std::cout << "s1.get(a)=" << (v1 ? *v1 : "(nil)") << "\n";
  std::cout << "s2.get(a)=" << (v2 ? *v2 : "(nil)") << "\n";
  std::cout << "s3.get(a)=" << (v3 ? *v3 : "(nil)") << "\n";

  n1.Stop(); n2.Stop(); n3.Stop();
  return 0;
}