#include "kv/kv_store.h"
#include <iostream>
#include <sstream>
#include <string>

int main() {
  std::cerr << "CLI started\n" << std::flush;

  kv::KVStore store;
  bool ok = store.open("/tmp/kv.wal");
  //std::cerr << "open(/tmp/kv.wal) -> " << (ok ? "true" : "false") << "\n" << std::flush;
  if (!ok) return 1;

  std::string line;
  while (true) {
    std::cerr << "kv> " << std::flush;

    if (!std::getline(std::cin, line)) {
      std::cerr << "\nEOF\n" << std::flush;
      break;
    }

    std::istringstream iss(line);
    std::string cmd;
    iss >> cmd;

    if (cmd == "PUT") {
      std::string k, v;
      iss >> k >> v;
      store.put(k, v);
      std::cerr << "OK\n" << std::flush;
    } else if (cmd == "GET") {
      std::string k;
      iss >> k;
      auto v = store.get(k);
      std::cerr << (v ? *v : "(nil)") << "\n" << std::flush;
    } else if (cmd == "EXIT") {
      std::cerr << "bye\n" << std::flush;
      break;
    } else if (cmd == "SNAP") {
       bool ok = store.checkpoint("/tmp/kv.snapshot", "/tmp/kv.wal");
       std::cerr << (ok ? "SNAP OK\n" : "SNAP FAIL\n");
    }else if (cmd == "FLUSH") {
       bool ok = store.flush_wal();
       std::cerr << (ok ? "FLUSH OK\n" : "FLUSH FAIL\n") << std::flush;
    }else {
        std::cerr << "Unknown\n" << std::flush;
    }
  }
  return 0;
}