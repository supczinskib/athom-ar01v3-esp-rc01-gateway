#include <array>
#include <cassert>
#include <cstdint>
#include <vector>

#include "esphome/components/fujitsu_vertical/fujitsu_vertical.h"

int main() {
  esphome::remote_transmitter::RemoteTransmitterComponent transmitter;
  esphome::fujitsu_vertical::FujitsuVerticalClimate climate;
  climate.set_transmitter(&transmitter);
  climate.transmit_nudge();

  const std::array<uint8_t, 7> frame{0x14, 0x63, 0x00, 0x10, 0x10, 0x6C, 0x93};
  std::vector<int32_t> expected{3300, -1600};
  for (const uint8_t byte : frame) {
    for (uint8_t mask = 0x01; mask != 0; mask <<= 1) {
      expected.push_back(420);
      expected.push_back((byte & mask) != 0 ? -1200 : -420);
    }
  }
  expected.push_back(420);
  expected.push_back(-8000);
  assert(transmitter.last_data() == expected);
  return 0;
}
