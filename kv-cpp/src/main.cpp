#include "kv/kv_store.h"
#include <iostream>
#include <sstream>

int main() {
  kv::KVStore store;

  // Use an absolute path to avoid “where did my WAL go?” issues.
  if (!store.open("/tmp/kv.wal")) {
    std::cerr << "ERROR: failed to open WAL at /tmp/kv.wal\n";
    return 1;
  }

  std::string line;
  while (true) {
    std::cout << "kv> ";
    if (!std::getline(std::cin, line)) break;

    std::istringstream iss(line);
    std::string cmd;
    iss >> cmd;

    if (cmd == "PUT") {
      std::string k, v;
      iss >> k >> v;
      store.put(k, v);
      std::cout << "OK\n";
    } else if (cmd == "GET") {
      std::string k;
      iss >> k;
      auto v = store.get(k);
      std::cout << (v ? *v : "(nil)") << "\n";
    } else if (cmd == "DEL") {
      std::string k;
      iss >> k;
      std::cout << (store.del(k) ? "1" : "0") << "\n";
    } else if (cmd == "SIZE") {
      std::cout << store.size() << "\n";
    } else if (cmd == "EXIT") {
      break;
    }
  }
}