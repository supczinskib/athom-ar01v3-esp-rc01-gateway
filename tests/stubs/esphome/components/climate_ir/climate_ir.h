#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string>

#include "esphome/components/remote_transmitter/remote_transmitter.h"
#include "esphome/core/component.h"

namespace esphome {

template<typename T> T clamp(T value, T minimum, T maximum) {
  return std::min(std::max(value, minimum), maximum);
}

namespace climate {

enum ClimateMode {
  CLIMATE_MODE_OFF,
  CLIMATE_MODE_HEAT_COOL,
  CLIMATE_MODE_COOL,
  CLIMATE_MODE_HEAT,
  CLIMATE_MODE_DRY,
  CLIMATE_MODE_FAN_ONLY,
};

enum ClimateFanMode {
  CLIMATE_FAN_ON,
  CLIMATE_FAN_AUTO,
  CLIMATE_FAN_LOW,
  CLIMATE_FAN_MEDIUM,
  CLIMATE_FAN_HIGH,
  CLIMATE_FAN_QUIET,
};

enum ClimateSwingMode {
  CLIMATE_SWING_OFF,
  CLIMATE_SWING_VERTICAL,
  CLIMATE_SWING_HORIZONTAL,
  CLIMATE_SWING_BOTH,
};

class ClimateCall {
 public:
  bool has_custom_preset() const { return false; }
  std::string get_custom_preset() const { return {}; }
};

}  // namespace climate

namespace remote_base {

class RemoteReceiveData {
 public:
  bool expect_item(uint32_t, uint32_t) { return false; }
};

}  // namespace remote_base

namespace climate_ir {

class ClimateIR : public Component {
 public:
  ClimateIR(float, float, float, bool, bool, std::initializer_list<climate::ClimateFanMode>,
            std::initializer_list<climate::ClimateSwingMode>) {}
  virtual ~ClimateIR() = default;

  void set_transmitter(remote_transmitter::RemoteTransmitterComponent *transmitter) { this->transmitter_ = transmitter; }
  void set_supported_custom_presets(std::initializer_list<const char *>) {}
  void publish_state() {}

  climate::ClimateMode mode{climate::CLIMATE_MODE_OFF};
  float target_temperature{24.0f};
  std::optional<climate::ClimateFanMode> fan_mode{climate::CLIMATE_FAN_AUTO};
  climate::ClimateSwingMode swing_mode{climate::CLIMATE_SWING_OFF};

 protected:
  virtual void control(const climate::ClimateCall &) {}
  virtual void transmit_state() = 0;
  virtual bool on_receive(remote_base::RemoteReceiveData) { return false; }

  remote_transmitter::RemoteTransmitterComponent *transmitter_{nullptr};
};

}  // namespace climate_ir
}  // namespace esphome
