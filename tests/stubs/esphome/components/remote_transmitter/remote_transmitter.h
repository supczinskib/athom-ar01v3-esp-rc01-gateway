#pragma once
#include <cstdint>
#include "esphome/components/remote_base/rc_switch_protocol.h"
namespace esphome::remote_transmitter {
class RemoteTransmitterComponent {
 public:
  class Call {
   public:
    remote_base::RemoteTransmitData *get_data() { return &data_; }
    void set_send_times(uint32_t) {}
    void set_send_wait(uint32_t) {}
    void perform() {}
   private:
    remote_base::RemoteTransmitData data_;
  };
  void set_carrier_duty_percent(uint8_t) {}
  Call transmit() { return {}; }
};
}
