#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>

std::string send_command(int port, const std::string& cmd) {
  int fd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) return "ERR socket\n";

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(static_cast<uint16_t>(port));
  inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

  if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
    ::close(fd);
    return "ERR connect\n";
  }

  std::string line = cmd + "\n";
  ::write(fd, line.data(), line.size());

  char buf[4096];
  ssize_t n = ::read(fd, buf, sizeof(buf) - 1);
  ::close(fd);

  if (n <= 0) return "ERR read\n";
  buf[n] = '\0';
  return std::string(buf);
}

int main(int argc, char* argv[]) {
  if (argc < 3) {
    std::cerr << "usage: kv_client <port> \"COMMAND\"\n";
    return 1;
  }

  int port = std::atoi(argv[1]);
  std::string cmd = argv[2];

  std::string resp = send_command(port, cmd);
  std::cout << "response: " << resp;

  if (resp.rfind("NOT_LEADER ", 0) == 0) {
    std::istringstream iss(resp);
    std::string tag;
    int leader_port = -1;
    iss >> tag >> leader_port;

    if (leader_port > 0) {
      std::cout << "redirecting to " << leader_port << "\n";
      std::string resp2 = send_command(leader_port, cmd);
      std::cout << "response: " << resp2;
    }
  }

  return 0;
}