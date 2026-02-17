#include "kv/kv_store.h"
#include <iostream>
#include <sstream>

int main() {
  kv::KVStore store;
  std::string line;

  while (true) {
    std::cout << "kv> ";
    std::getline(std::cin, line);
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
      std::cout << store.del(k) << "\n";
    } else if (cmd == "SIZE") {
      std::cout << store.size() << "\n";
    } else if (cmd == "EXIT") {
      break;
    }
  }
}
