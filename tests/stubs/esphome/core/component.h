#pragma once
#include <functional>
namespace esphome {
namespace setup_priority { static constexpr float AFTER_WIFI = 100.0f; }
class Component {
 public:
  virtual ~Component() = default;
  virtual void setup() {}
  virtual void dump_config() {}
  virtual float get_setup_priority() const { return 0; }
  template<typename F> void defer(F &&fn) { fn(); }
};
}
