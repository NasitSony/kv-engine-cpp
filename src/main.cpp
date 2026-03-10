#include "kv/kv_store.h"
#include <iostream>
#include <sstream>
#include <string>
#include <chrono>

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
    } else if (cmd == "BENCH") {
      int N = 100000;            // start with 100k; drop to 10k if too slow
      iss >> N;                  // allow: BENCH 10000

      // optional: keep values fixed to avoid extra allocations
      std::string value = "v";

     // Warmup (optional)
     for (int i = 0; i < 1000; i++) {
        store.put("warm" + std::to_string(i), value);
     }
     store.flush_wal(); // ensure warmup is durable so it doesn't pollute measurement

     auto start = std::chrono::high_resolution_clock::now();

     for (int i = 0; i < N; i++) {
        store.put("k" + std::to_string(i), value);
      }

     // IMPORTANT: include durability boundary in timing
     store.flush_wal();

     auto end = std::chrono::high_resolution_clock::now();
     auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

     double ops_per_sec = (ms > 0) ? (1000.0 * N / (double)ms) : 0.0;

     std::cerr << "BENCH N=" << N
            << " total_ms=" << ms
            << " ops_per_sec=" << ops_per_sec
            << "\n" << std::flush;
}else if (cmd == "SETBATCH") {
  int n;
  iss >> n;
  store.set_group_commit_every(n);
  std::cerr << "OK batch=" << n << "\n" << std::flush;
}else if (cmd == "GET") {
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