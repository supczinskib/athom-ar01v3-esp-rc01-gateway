#include "fujitsu_vertical.h"

namespace esphome::fujitsu_vertical {

#define SET_NIBBLE(message, nibble, value) \
  ((message)[(nibble) / 2] |= ((value) &0x0F) << (((nibble) % 2) ? 0 : 4))
#define GET_NIBBLE(message, nibble) (((message)[(nibble) / 2] >> (((nibble) % 2) ? 0 : 4)) & 0x0F)

static const char *const TAG = "fujitsu_vertical.climate";

static constexpr uint8_t COMMON_LENGTH = 6;
static constexpr uint8_t COMMON_BYTE0 = 0x14;
static constexpr uint8_t COMMON_BYTE1 = 0x63;
static constexpr uint8_t COMMON_BYTE2 = 0x00;
static constexpr uint8_t COMMON_BYTE3 = 0x10;
static constexpr uint8_t COMMON_BYTE4 = 0x10;
static constexpr uint8_t MESSAGE_TYPE_BYTE = 5;

static constexpr uint8_t STATE_MESSAGE_LENGTH = 16;
static constexpr uint8_t MESSAGE_TYPE_STATE = 0xFE;
static constexpr uint8_t UTIL_MESSAGE_LENGTH = 7;
static constexpr uint8_t MESSAGE_TYPE_OFF = 0x02;
static constexpr uint8_t MESSAGE_TYPE_ECONOMY = 0x09;
static constexpr uint8_t MESSAGE_TYPE_NUDGE = 0x6C;

static constexpr uint8_t STATE_HEADER_BYTE0 = 0x09;
static constexpr uint8_t STATE_HEADER_BYTE1 = 0x30;
static constexpr uint8_t STATE_FOOTER_BYTE0 = 0x20;
static constexpr uint8_t TEMPERATURE_NIBBLE = 16;
static constexpr uint8_t POWER_ON_NIBBLE = 17;
static constexpr uint8_t POWER_ON = 0x01;
static constexpr uint8_t MODE_NIBBLE = 19;
static constexpr uint8_t MODE_AUTO = 0x00;
static constexpr uint8_t MODE_COOL = 0x01;
static constexpr uint8_t MODE_DRY = 0x02;
static constexpr uint8_t MODE_FAN = 0x03;
static constexpr uint8_t MODE_HEAT = 0x04;
static constexpr uint8_t SWING_NIBBLE = 20;
static constexpr uint8_t SWING_NONE = 0x00;
static constexpr uint8_t SWING_VERTICAL = 0x01;
static constexpr uint8_t FAN_NIBBLE = 21;
static constexpr uint8_t FAN_AUTO = 0x00;
static constexpr uint8_t FAN_HIGH = 0x01;
static constexpr uint8_t FAN_MEDIUM = 0x02;
static constexpr uint8_t FAN_LOW = 0x03;
static constexpr uint8_t FAN_SILENT = 0x04;

static constexpr uint16_t HEADER_MARK = 3300;
static constexpr uint16_t HEADER_SPACE = 1600;
static constexpr uint16_t BIT_MARK = 420;
static constexpr uint16_t ONE_SPACE = 1200;
static constexpr uint16_t ZERO_SPACE = 420;
static constexpr uint16_t TRAILER_MARK = 420;
static constexpr uint16_t TRAILER_SPACE = 8000;
static constexpr uint32_t CARRIER_FREQUENCY = 38000;

void FujitsuVerticalClimate::transmit_state() {
  if (this->mode == climate::CLIMATE_MODE_OFF) {
    this->transmit_off_();
    return;
  }

  uint8_t state[STATE_MESSAGE_LENGTH] = {0};
  state[0] = COMMON_BYTE0;
  state[1] = COMMON_BYTE1;
  state[2] = COMMON_BYTE2;
  state[3] = COMMON_BYTE3;
  state[4] = COMMON_BYTE4;
  state[5] = MESSAGE_TYPE_STATE;
  state[6] = STATE_HEADER_BYTE0;
  state[7] = STATE_HEADER_BYTE1;
  state[14] = STATE_FOOTER_BYTE0;

  const uint8_t temperature =
      static_cast<uint8_t>(roundf(clamp<float>(this->target_temperature, FUJITSU_VERTICAL_TEMP_MIN,
                                               FUJITSU_VERTICAL_TEMP_MAX)));
  SET_NIBBLE(state, TEMPERATURE_NIBBLE, temperature - FUJITSU_VERTICAL_TEMP_MIN);
  if (!this->power_)
    SET_NIBBLE(state, POWER_ON_NIBBLE, POWER_ON);

  switch (this->mode) {
    case climate::CLIMATE_MODE_COOL:
      SET_NIBBLE(state, MODE_NIBBLE, MODE_COOL);
      break;
    case climate::CLIMATE_MODE_HEAT:
      SET_NIBBLE(state, MODE_NIBBLE, MODE_HEAT);
      break;
    case climate::CLIMATE_MODE_DRY:
      SET_NIBBLE(state, MODE_NIBBLE, MODE_DRY);
      break;
    case climate::CLIMATE_MODE_FAN_ONLY:
      SET_NIBBLE(state, MODE_NIBBLE, MODE_FAN);
      break;
    case climate::CLIMATE_MODE_HEAT_COOL:
    default:
      SET_NIBBLE(state, MODE_NIBBLE, MODE_AUTO);
      break;
  }

  switch (this->fan_mode.value_or(climate::CLIMATE_FAN_AUTO)) {
    case climate::CLIMATE_FAN_HIGH:
      SET_NIBBLE(state, FAN_NIBBLE, FAN_HIGH);
      break;
    case climate::CLIMATE_FAN_MEDIUM:
      SET_NIBBLE(state, FAN_NIBBLE, FAN_MEDIUM);
      break;
    case climate::CLIMATE_FAN_LOW:
      SET_NIBBLE(state, FAN_NIBBLE, FAN_LOW);
      break;
    case climate::CLIMATE_FAN_QUIET:
      SET_NIBBLE(state, FAN_NIBBLE, FAN_SILENT);
      break;
    case climate::CLIMATE_FAN_AUTO:
    default:
      SET_NIBBLE(state, FAN_NIBBLE, FAN_AUTO);
      break;
  }

  SET_NIBBLE(state, SWING_NIBBLE,
             this->swing_mode == climate::CLIMATE_SWING_VERTICAL ? SWING_VERTICAL : SWING_NONE);
  state[STATE_MESSAGE_LENGTH - 1] = this->checksum_state_(state);
  this->transmit_(state, STATE_MESSAGE_LENGTH);
  this->power_ = true;
}

void FujitsuVerticalClimate::transmit_off_() {
  uint8_t state[UTIL_MESSAGE_LENGTH] = {COMMON_BYTE0, COMMON_BYTE1, COMMON_BYTE2, COMMON_BYTE3,
                                        COMMON_BYTE4, MESSAGE_TYPE_OFF, 0};
  state[UTIL_MESSAGE_LENGTH - 1] = this->checksum_util_(state);
  this->transmit_(state, UTIL_MESSAGE_LENGTH);
  this->power_ = false;
}

void FujitsuVerticalClimate::transmit_nudge() {
  uint8_t state[UTIL_MESSAGE_LENGTH] = {COMMON_BYTE0, COMMON_BYTE1, COMMON_BYTE2, COMMON_BYTE3,
                                        COMMON_BYTE4, MESSAGE_TYPE_NUDGE, 0};
  state[UTIL_MESSAGE_LENGTH - 1] = this->checksum_util_(state);
  ESP_LOGD(TAG, "Transmit vertical vane SET command");
  this->transmit_(state, UTIL_MESSAGE_LENGTH);
}

void FujitsuVerticalClimate::transmit_(const uint8_t *message, uint8_t length) {
  auto transmit = this->transmitter_->transmit();
  auto *data = transmit.get_data();
  data->set_carrier_frequency(CARRIER_FREQUENCY);
  data->mark(HEADER_MARK);
  data->space(HEADER_SPACE);
  for (uint8_t i = 0; i < length; ++i) {
    for (uint8_t mask = 0x01; mask != 0; mask <<= 1) {
      data->mark(BIT_MARK);
      data->space((message[i] & mask) ? ONE_SPACE : ZERO_SPACE);
    }
  }
  data->mark(TRAILER_MARK);
  data->space(TRAILER_SPACE);
  transmit.perform();
}

uint8_t FujitsuVerticalClimate::checksum_state_(const uint8_t *message) {
  uint8_t checksum = 0;
  for (uint8_t i = 7; i < STATE_MESSAGE_LENGTH - 1; ++i)
    checksum += message[i];
  return 256 - checksum;
}

uint8_t FujitsuVerticalClimate::checksum_util_(const uint8_t *message) { return 255 - message[5]; }

bool FujitsuVerticalClimate::on_receive(remote_base::RemoteReceiveData data) {
  if (!data.expect_item(HEADER_MARK, HEADER_SPACE))
    return false;

  uint8_t message[STATE_MESSAGE_LENGTH] = {0};
  for (uint8_t byte = 0; byte < COMMON_LENGTH; ++byte) {
    for (uint8_t bit = 0; bit < 8; ++bit) {
      if (data.expect_item(BIT_MARK, ONE_SPACE))
        message[byte] |= 1 << bit;
      else if (!data.expect_item(BIT_MARK, ZERO_SPACE))
        return false;
    }
  }

  const uint8_t type = message[MESSAGE_TYPE_BYTE];
  const uint8_t length = type == MESSAGE_TYPE_STATE ? STATE_MESSAGE_LENGTH : UTIL_MESSAGE_LENGTH;
  if (type != MESSAGE_TYPE_STATE && type != MESSAGE_TYPE_OFF && type != MESSAGE_TYPE_ECONOMY &&
      type != MESSAGE_TYPE_NUDGE)
    return false;

  for (uint8_t byte = COMMON_LENGTH; byte < length; ++byte) {
    for (uint8_t bit = 0; bit < 8; ++bit) {
      if (data.expect_item(BIT_MARK, ONE_SPACE))
        message[byte] |= 1 << bit;
      else if (!data.expect_item(BIT_MARK, ZERO_SPACE))
        return false;
    }
  }

  const uint8_t checksum =
      type == MESSAGE_TYPE_STATE ? this->checksum_state_(message) : this->checksum_util_(message);
  if (message[length - 1] != checksum)
    return false;

  if (type == MESSAGE_TYPE_NUDGE) {
    ESP_LOGD(TAG, "Received vertical vane SET command");
    return true;
  }
  if (type == MESSAGE_TYPE_ECONOMY)
    return false;
  if (type == MESSAGE_TYPE_OFF) {
    this->mode = climate::CLIMATE_MODE_OFF;
    this->power_ = false;
    this->publish_state();
    return true;
  }

  this->target_temperature = GET_NIBBLE(message, TEMPERATURE_NIBBLE) + FUJITSU_VERTICAL_TEMP_MIN;
  switch (GET_NIBBLE(message, MODE_NIBBLE)) {
    case MODE_COOL:
      this->mode = climate::CLIMATE_MODE_COOL;
      break;
    case MODE_HEAT:
      this->mode = climate::CLIMATE_MODE_HEAT;
      break;
    case MODE_DRY:
      this->mode = climate::CLIMATE_MODE_DRY;
      break;
    case MODE_FAN:
      this->mode = climate::CLIMATE_MODE_FAN_ONLY;
      break;
    default:
      this->mode = climate::CLIMATE_MODE_HEAT_COOL;
      break;
  }

  switch (GET_NIBBLE(message, FAN_NIBBLE)) {
    case FAN_SILENT:
      this->fan_mode = climate::CLIMATE_FAN_QUIET;
      break;
    case FAN_LOW:
      this->fan_mode = climate::CLIMATE_FAN_LOW;
      break;
    case FAN_MEDIUM:
      this->fan_mode = climate::CLIMATE_FAN_MEDIUM;
      break;
    case FAN_HIGH:
      this->fan_mode = climate::CLIMATE_FAN_HIGH;
      break;
    default:
      this->fan_mode = climate::CLIMATE_FAN_AUTO;
      break;
  }

  this->swing_mode = (GET_NIBBLE(message, SWING_NIBBLE) & SWING_VERTICAL) != 0
                         ? climate::CLIMATE_SWING_VERTICAL
                         : climate::CLIMATE_SWING_OFF;
  this->power_ = true;
  this->publish_state();
  return true;
}

}  // namespace esphome::fujitsu_vertical
