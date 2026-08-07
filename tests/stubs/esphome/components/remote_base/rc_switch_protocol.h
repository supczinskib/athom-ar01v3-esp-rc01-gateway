#pragma once

#include <cstdint>
#include <vector>

namespace esphome::remote_base {

class RemoteTransmitData {
 public:
  void mark(uint32_t length) { data_.push_back(static_cast<int32_t>(length)); }
  void space(uint32_t length) { data_.push_back(-static_cast<int32_t>(length)); }
  void set_carrier_frequency(uint32_t) {}
  void set_data(const std::vector<int32_t> &data) { data_ = data; }
  const std::vector<int32_t> &get_data() const { return data_; }

 private:
  std::vector<int32_t> data_;
};

class RCSwitchBase {
 public:
  constexpr RCSwitchBase(uint32_t sync_high, uint32_t sync_low, uint32_t zero_high,
                         uint32_t zero_low, uint32_t one_high, uint32_t one_low, bool inverted)
      : sync_high_(sync_high), sync_low_(sync_low), zero_high_(zero_high), zero_low_(zero_low),
        one_high_(one_high), one_low_(one_low), inverted_(inverted) {}

  void item(RemoteTransmitData *dst, uint32_t high, uint32_t low) const {
    if (!inverted_) {
      dst->mark(high);
      dst->space(low);
    } else {
      dst->space(high);
      dst->mark(low);
    }
  }

  void transmit(RemoteTransmitData *dst, uint64_t code, uint8_t len) const {
    item(dst, sync_high_, sync_low_);
    for (int16_t bit = len - 1; bit >= 0; --bit) {
      if ((code & (uint64_t{1} << bit)) != 0)
        item(dst, one_high_, one_low_);
      else
        item(dst, zero_high_, zero_low_);
    }
  }

 private:
  uint32_t sync_high_;
  uint32_t sync_low_;
  uint32_t zero_high_;
  uint32_t zero_low_;
  uint32_t one_high_;
  uint32_t one_low_;
  bool inverted_;
};

inline constexpr RCSwitchBase RC_SWITCH_PROTOCOLS[9] = {
    RCSwitchBase(0, 0, 0, 0, 0, 0, false),
    RCSwitchBase(350, 10850, 350, 1050, 1050, 350, false),
    RCSwitchBase(650, 6500, 650, 1300, 1300, 650, false),
    RCSwitchBase(3000, 7100, 400, 1100, 900, 600, false),
    RCSwitchBase(380, 2280, 380, 1140, 1140, 380, false),
    RCSwitchBase(3000, 7000, 500, 1000, 1000, 500, false),
    RCSwitchBase(10350, 450, 450, 900, 900, 450, true),
    RCSwitchBase(300, 9300, 150, 900, 900, 150, false),
    RCSwitchBase(250, 2500, 250, 1250, 250, 250, false),
};

}  // namespace esphome::remote_base
