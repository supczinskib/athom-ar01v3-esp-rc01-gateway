#pragma once

#include "esphome/components/climate_ir/climate_ir.h"
#include "esphome/core/component.h"
#include "esphome/core/log.h"

namespace esphome::fujitsu_vertical {

static constexpr uint8_t FUJITSU_VERTICAL_TEMP_MIN = 16;
static constexpr uint8_t FUJITSU_VERTICAL_TEMP_MAX = 30;

class FujitsuVerticalClimate final : public climate_ir::ClimateIR {
 public:
  FujitsuVerticalClimate()
      : ClimateIR(FUJITSU_VERTICAL_TEMP_MIN, FUJITSU_VERTICAL_TEMP_MAX, 1.0f, true, true,
                  {climate::CLIMATE_FAN_AUTO, climate::CLIMATE_FAN_LOW, climate::CLIMATE_FAN_MEDIUM,
                   climate::CLIMATE_FAN_HIGH, climate::CLIMATE_FAN_QUIET},
                  {climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_VERTICAL}) {}

  void transmit_nudge();

 protected:
  void transmit_state() override;
  void transmit_off_();
  bool on_receive(remote_base::RemoteReceiveData data) override;
  void transmit_(const uint8_t *message, uint8_t length);
  uint8_t checksum_state_(const uint8_t *message);
  uint8_t checksum_util_(const uint8_t *message);

  bool power_{false};
};

}  // namespace esphome::fujitsu_vertical
