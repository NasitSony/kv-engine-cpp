#include "kv/kv_store.h"
#include "kv/raft.h"
#include "kv/raft_transport.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {
int port_for_node(int node_id) {
  switch (node_id) {
    case 1: return 9101;
    case 2: return 9102;
    case 3: return 9103;
    default: return -1;
  }
}

std::string handle_command(kv::KVStore& store,
                           kv::RaftNode* raft,
                           int my_port,
                           const std::string& line) {
  std::istringstream iss(line);
  std::string cmd;
  iss >> cmd;

  bool is_leader = raft && (raft->role() == kv::RaftRole::Leader);

  if (cmd == "PUT") {
    std::string k, v;
    iss >> k >> v;
    if (k.empty() || v.empty()) return "ERR usage: PUT key value\n";
    if (!is_leader) {
       int leader_port = -1;
       if (raft && raft->leader_id().has_value()) {
         leader_port = port_for_node(*raft->leader_id());
        }
        if (leader_port > 0) {
           return "NOT_LEADER " + std::to_string(leader_port) + "\n";
        }
        return "NOT_LEADER UNKNOWN\n";
    }
    bool ok = raft->ProposePut(k, v);
    return ok ? "OK\n" : "FAIL\n";
  }

  if (cmd == "GET") {
    std::string k;
    iss >> k;
    if (k.empty()) return "ERR usage: GET key\n";
    auto v = store.get(k);
    return v ? ("VALUE " + *v + "\n") : "NOT_FOUND\n";
  }

  if (cmd == "DEL") {
    std::string k;
    iss >> k;
    if (k.empty()) return "ERR usage: DEL key\n";
    if (!is_leader) {
       int leader_port = -1;
       if (raft && raft->leader_id().has_value()) {
         leader_port = port_for_node(*raft->leader_id());
        }
        if (leader_port > 0) {
           return "NOT_LEADER " + std::to_string(leader_port) + "\n";
        }
        return "NOT_LEADER UNKNOWN\n";
    }
    bool ok = raft->ProposeDel(k);
    return ok ? "OK\n" : "NOT_FOUND\n";
  }

  if (cmd == "SIZE") {
    return "SIZE " + std::to_string(store.size()) + "\n";
  }

  if (cmd == "STATUS") {
    if (!raft) return "ROLE UNKNOWN\n";
    return is_leader ? "ROLE LEADER\n" : "ROLE FOLLOWER\n";
  }

  if (cmd == "PING") {
    return "PONG\n";
  }

  if (cmd == "KILL") {
    if (!raft) return "ERR no raft\n";
    raft->Stop();
    return "STOPPED\n";
}

  return "ERR unknown command\n";
}

void run_server(int port, kv::KVStore* store, kv::RaftNode* raft) {
  int server_fd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) {
    std::cerr << "socket() failed on port " << port << "\n";
    return;
  }

  int opt = 1;
  ::setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(static_cast<uint16_t>(port));

  if (::bind(server_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
    std::cerr << "bind() failed on port " << port << "\n";
    ::close(server_fd);
    return;
  }

  if (::listen(server_fd, 16) < 0) {
    std::cerr << "listen() failed on port " << port << "\n";
    ::close(server_fd);
    return;
  }

  std::cout << "server listening on port " << port << "\n";

  while (true) {
    int client_fd = ::accept(server_fd, nullptr, nullptr);
    if (client_fd < 0) continue;

    char buffer[4096];
    std::string pending;

    while (true) {
      ssize_t n = ::read(client_fd, buffer, sizeof(buffer));
      if (n <= 0) break;

      pending.append(buffer, static_cast<size_t>(n));

      size_t pos = 0;
      while (true) {
        size_t nl = pending.find('\n', pos);
        if (nl == std::string::npos) {
          pending.erase(0, pos);
          break;
        }

        std::string line = pending.substr(pos, nl - pos);
        if (!line.empty() && line.back() == '\r') line.pop_back();

        std::string resp = handle_command(*store, raft, port, line);
        ::write(client_fd, resp.data(), resp.size());

        pos = nl + 1;
      }
    }

    ::close(client_fd);
  }
}

}  // namespace

int main() {
  kv::InProcessTransport t;

  kv::KVStore s1, s2, s3;

  auto n1 = std::make_unique<kv::RaftNode>(1, std::vector<int>{2, 3}, &t, &s1);
  auto n2 = std::make_unique<kv::RaftNode>(2, std::vector<int>{1, 3}, &t, &s2);
  auto n3 = std::make_unique<kv::RaftNode>(3, std::vector<int>{1, 2}, &t, &s3);

  t.Register(n1.get());
  t.Register(n2.get());
  t.Register(n3.get());

  n1->Start();
  n2->Start();
  n3->Start();

  std::thread th1(run_server, 9101, &s1, n1.get());
  std::thread th2(run_server, 9102, &s2, n2.get());
  std::thread th3(run_server, 9103, &s3, n3.get());

  th1.join();
  th2.join();
  th3.join();

  return 0;
}