#include "kv/kv_store.h"
#include "kv/raft.h"


#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>

namespace {

std::string handle_command(kv::KVStore& store,
                           kv::RaftNode* raft,
                           const std::string& line){
  
  bool is_leader = raft && (raft->role() == kv::RaftRole::Leader);
  std::istringstream iss(line);
  std::string cmd;
  iss >> cmd;

  if (cmd == "PUT") {
    if (!is_leader) return "NOT_LEADER\n";
    std::string k, v;
    iss >> k >> v;
    if (k.empty() || v.empty()) return "ERR usage: PUT key value\n";
    if (!is_leader) return "NOT_LEADER\n";
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
    if (!is_leader) return "NOT_LEADER\n";
    std::string k;
    iss >> k;
    if (k.empty()) return "ERR usage: DEL key\n";
    if (!is_leader) return "NOT_LEADER\n";

    bool ok = raft->ProposeDel(k);
    return ok ? "OK\n" : "NOT_FOUND\n";
  }

  if (cmd == "SIZE") {
    return "SIZE " + std::to_string(store.size()) + "\n";
  }

  if (cmd == "FLUSH") {
    bool ok = store.flush_wal();
    return ok ? "FLUSH_OK\n" : "FLUSH_FAIL\n";
  }

  if (cmd == "PING") {
    return "PONG\n";
  }

  if (cmd == "STATUS") {
    if (!raft) return "ROLE UNKNOWN\n";
    return is_leader ? "ROLE LEADER\n" : "ROLE FOLLOWER\n";
  }

  return "ERR unknown command\n";
}

}  // namespace

int main(int argc, char* argv[]) {
  int port = 9000;
  if (argc >= 2) {
    port = std::atoi(argv[1]);
  }

  kv::KVStore store;
  if (!store.open("/tmp/kv_server.wal")) {
    std::cerr << "failed to open WAL\n";
    return 1;
  }
  kv::RaftNode* raft = nullptr; // placeholder for now

  int server_fd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) {
    std::cerr << "socket() failed\n";
    return 1;
  }

  int opt = 1;
  ::setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(static_cast<uint16_t>(port));

  if (::bind(server_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
    std::cerr << "bind() failed\n";
    ::close(server_fd);
    return 1;
  }

  if (::listen(server_fd, 16) < 0) {
    std::cerr << "listen() failed\n";
    ::close(server_fd);
    return 1;
  }

  std::cout << "kv_server listening on port " << port << "\n";

  while (true) {
    int client_fd = ::accept(server_fd, nullptr, nullptr);
    if (client_fd < 0) {
      continue;
    }

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

        bool is_leader = true; // single-node mode for now
        std::string resp = handle_command(store, raft, line);
        ::write(client_fd, resp.data(), resp.size());

        pos = nl + 1;
      }
    }

    ::close(client_fd);
  }

  ::close(server_fd);
  return 0;
}