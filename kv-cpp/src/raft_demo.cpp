#include "kv/raft_transport.h"
#include "kv/kv_store.h"

#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

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
  auto n1 = std::make_unique<kv::RaftNode>(1, std::vector<int>{2, 3}, &t, &s1);
  auto n2 = std::make_unique<kv::RaftNode>(2, std::vector<int>{1, 3}, &t, &s2);
  auto n3 = std::make_unique<kv::RaftNode>(3, std::vector<int>{1, 2}, &t, &s3);
  t.Register(n1.get());
  t.Register(n2.get());
  t.Register(n3.get());

  std::vector<kv::RaftNode*> nodes = {n1.get(), n2.get(), n3.get()};

  n1->Start();
  n2->Start();
  n3->Start();

  // Wait for initial leader election
  kv::RaftNode* leader = nullptr;
  for (int i = 0; i < 40 && !leader; i++) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    if (n1->role() == kv::RaftRole::Leader) leader = n1.get();
    else if (n2->role() == kv::RaftRole::Leader) leader = n2.get();
    else if (n3->role() == kv::RaftRole::Leader) leader = n3.get();

    std::cout << "n1(" << role_name(n1->role()) << ",t=" << n1->term() << ") "
              << "n2(" << role_name(n2->role()) << ",t=" << n2->term() << ") "
              << "n3(" << role_name(n3->role()) << ",t=" << n3->term() << ")\n";
  }

  if (!leader) {
    std::cerr << "No leader elected\n";
    n1->Stop();
    n2->Stop();
    n3->Stop();
    return 1;
  }

  std::cout << "Leader is node " << leader->id() << "\n";

  bool ok1 = leader->ProposePut("a", "100");
  std::cout << "ProposePut(a=100) -> " << (ok1 ? "true" : "false") << "\n";

  std::this_thread::sleep_for(std::chrono::milliseconds(400));

  std::cout << "s1.get(a)=" << s1.get("a").value_or("(nil)") << "\n";
  std::cout << "s2.get(a)=" << s2.get("a").value_or("(nil)") << "\n";
  std::cout << "s3.get(a)=" << s3.get("a").value_or("(nil)") << "\n";

  std::cout << "\n=== Simulating leader crash ===\n";
  leader->Stop();
  t.Unregister(leader->id());

  // Wait for re-election
  kv::RaftNode* new_leader = nullptr;
  for (int i = 0; i < 50 && !new_leader; i++) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    for (auto* n : nodes) {
      if (n == leader) continue;
      if (n->role() == kv::RaftRole::Leader) {
        new_leader = n;
        break;
      }
    }

    std::cout << "after crash: "
              << "n1(" << role_name(n1->role()) << ",t=" << n1->term() << ") "
              << "n2(" << role_name(n2->role()) << ",t=" << n2->term() << ") "
              << "n3(" << role_name(n3->role()) << ",t=" << n3->term() << ")\n";
  }

  if (!new_leader) {
    std::cerr << "No new leader elected after crash\n";
    n1->Stop();
    n2->Stop();
    n3->Stop();
    return 1;
  }

  std::cout << "New leader is node " << new_leader->id() << "\n";

  bool ok2 = new_leader->ProposePut("b", "200");
  std::cout << "ProposePut(b=200) -> " << (ok2 ? "true" : "false") << "\n";

  std::this_thread::sleep_for(std::chrono::milliseconds(500));

  std::cout << "s1.get(b)=" << s1.get("b").value_or("(nil)") << "\n";
  std::cout << "s2.get(b)=" << s2.get("b").value_or("(nil)") << "\n";
  std::cout << "s3.get(b)=" << s3.get("b").value_or("(nil)") << "\n";

  std::cout << "\n=== Restarting crashed node ===\n";

  int crashed_id = leader->id();

  // recreate the crashed RaftNode using the SAME KVStore
  if (crashed_id == 1) {
    n1 = std::make_unique<kv::RaftNode>(1, std::vector<int>{2, 3}, &t, &s1);
    t.Register(n1.get());
    n1->Start();
  } else if (crashed_id == 2) {
    n2 = std::make_unique<kv::RaftNode>(2, std::vector<int>{1, 3}, &t, &s2);
    t.Register(n2.get());
    n2->Start();
  } else if (crashed_id == 3) {
    n3 = std::make_unique<kv::RaftNode>(3, std::vector<int>{1, 2}, &t, &s3);
    t.Register(n3.get());
    n3->Start(); 
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(1000));

  std::cout << "after restart:\n";
  std::cout << "s1.get(b)=" << s1.get("b").value_or("(nil)") << "\n";
  std::cout << "s2.get(b)=" << s2.get("b").value_or("(nil)") << "\n";
  std::cout << "s3.get(b)=" << s3.get("b").value_or("(nil)") << "\n";
  // Stop remaining nodes safely
  if (n1.get() != leader) n1->Stop();
  if (n2.get() != leader) n2->Stop();
  if (n3.get() != leader) n3->Stop();

  return 0;
}