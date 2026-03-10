#pragma once
#include <string>

namespace kv {

class IRaftStateMachine {
 public:
  virtual ~IRaftStateMachine() = default;
  virtual void ApplyPut(std::string key, std::string value) = 0;
  virtual void ApplyDel(const std::string& key) = 0;
};

} // namespace kv