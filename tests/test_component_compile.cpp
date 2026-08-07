#include "../esphome/components/flipper_importer/flipper_importer.h"

int main() {
  esphome::flipper_importer::FlipperImporter importer(nullptr, nullptr);
  esphome::remote_transmitter::RemoteTransmitterComponent ir_transmitter;
  esphome::remote_transmitter::RemoteTransmitterComponent rf_transmitter;
  esphome::text_sensor::TextSensor ir_status;
  esphome::text_sensor::TextSensor rf_status;
  importer.set_ir_transmitter(&ir_transmitter);
  importer.set_rf_transmitter(&rf_transmitter);
  importer.set_ir_status(&ir_status);
  importer.set_rf_status(&rf_status);
  importer.set_ir_slot_status(0, &ir_status);
  importer.set_rf_slot_status(0, &rf_status);
  if (importer.ir_slot_preview(0) != "None" || importer.rf_slot_preview(0) != "None") return 1;

  const std::vector<int32_t> raw{9000, -4500, 560, -560};
  if (!importer.send_ir_raw_now(raw, 38000, 33, 1)) return 2;
  if (!importer.send_ir_nec_now(0x00, 0x2A, 1)) return 3;
  if (!importer.send_rf_raw_now(raw, 5, 0)) return 4;
  if (!importer.send_rf_princeton_now("0xFFFF01", 24, 403, 30, 5, 0)) return 5;
  if (!importer.send_rf_dooya_now("0x123456789A", 5)) return 6;

  if (importer.send_ir_nec_now(256, 0, 1)) return 7;
  if (importer.send_rf_princeton_now("0x1000000", 24, 403, 30, 5, 0)) return 8;
  if (importer.send_rf_dooya_now("0x1123456789A", 5)) return 9;
  return 0;
}
