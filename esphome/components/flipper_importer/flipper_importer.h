#pragma once

#include "esphome/components/remote_transmitter/remote_transmitter.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/web_server_base/web_server_base.h"
#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "../Flash_comp/Flash_comp.h"
#include "flipper_parser.h"

#include <array>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace esphome::flipper_importer {

class FlipperImporter final : public Component, public AsyncWebHandler {
 public:
  FlipperImporter(web_server_base::WebServerBase *base, flash_component::Flash_comp *storage)
      : base_(base), storage_(storage) {}

  void set_ir_transmitter(remote_transmitter::RemoteTransmitterComponent *value) { this->ir_transmitter_ = value; }
  void set_rf_transmitter(remote_transmitter::RemoteTransmitterComponent *value) { this->rf_transmitter_ = value; }
  void set_ir_status(text_sensor::TextSensor *value) { this->ir_status_ = value; }
  void set_rf_status(text_sensor::TextSensor *value) { this->rf_status_ = value; }
  void set_ir_slot_status(uint8_t slot, text_sensor::TextSensor *value) {
    if (slot < this->ir_slot_status_.size()) this->ir_slot_status_[slot] = value;
  }
  void set_rf_slot_status(uint8_t slot, text_sensor::TextSensor *value) {
    if (slot < this->rf_slot_status_.size()) this->rf_slot_status_[slot] = value;
  }

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  bool canHandle(AsyncWebServerRequest *request) const override;
  void handleRequest(AsyncWebServerRequest *request) override;
  void handleBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) override;
  void handleUpload(AsyncWebServerRequest *request, const std::string &filename, size_t index, uint8_t *data,
                    size_t len, bool final) override;
  bool isRequestHandlerTrivial() const override { return false; }

  bool save_ir_raw(uint8_t slot, const std::vector<int32_t> &timings, uint32_t frequency = 38000,
                   uint8_t duty_percent = 50, uint8_t recommended_repeats = 1);
  bool save_ir_nec(uint8_t slot, uint16_t address, uint16_t command);
  bool save_rf_raw(uint8_t slot, const std::vector<int32_t> &timings, uint32_t frequency = 433920000,
                   uint8_t recommended_repeats = 5);
  bool save_rf_rc_switch(uint8_t slot, uint64_t code, uint8_t protocol,
                         uint8_t recommended_repeats = 5);
  bool save_rf_captured_raw(uint8_t slot, const std::vector<int32_t> &timings,
                            uint32_t frequency = 433920000);
  int learn_rf_capture(uint8_t slot, const std::vector<int32_t> &capture, bool has_rc_switch,
                       uint64_t rc_code, uint8_t rc_protocol, uint8_t recommended_repeats,
                       bool fast_raw, std::string &message);
  bool clear_ir_slot(uint8_t slot);
  bool clear_rf_slot(uint8_t slot);
  bool send_ir_slot(uint8_t slot);
  bool send_rf_slot(uint8_t slot, uint32_t repeats, uint32_t wait_us);
  bool send_ir_raw_now(const std::vector<int32_t> &timings, uint32_t carrier_hz,
                       uint32_t duty_percent, uint32_t repeats);
  bool send_ir_raw_text_now(const std::string &timings, uint32_t carrier_hz,
                            uint32_t duty_percent, uint32_t repeats);
  bool send_ir_nec_now(uint32_t address, uint32_t command, uint32_t repeats);
  bool send_rf_raw_now(const std::vector<int32_t> &timings, uint32_t repeats,
                       uint32_t gap_ms);
  bool send_rf_raw_text_now(const std::string &timings, uint32_t repeats,
                            uint32_t gap_ms);
  bool send_rf_princeton_now(const std::string &code_hex, uint32_t bit_count,
                             uint32_t te_us, uint32_t guard_multiplier,
                             uint32_t repeats, uint32_t gap_ms);
  bool send_rf_dooya_now(const std::string &code_hex, uint32_t repeats);
  std::string ir_slot_preview(uint8_t slot);
  std::string rf_slot_preview(uint8_t slot);

 protected:
  static constexpr size_t MAX_UPLOAD_BYTES = MAX_FLIPPER_FILE_BYTES;

  void handle_index_(AsyncWebServerRequest *request);
  void handle_import_(AsyncWebServerRequest *request);
  void handle_test_(AsyncWebServerRequest *request);
  void handle_status_(AsyncWebServerRequest *request);
  void handle_capture_(AsyncWebServerRequest *request);
  static void send_text_response_(AsyncWebServerRequest *request, int code, const char *content_type,
                                  const std::string &body);
  static void send_ui_result_(AsyncWebServerRequest *request, int code, const std::string &message);
  bool save_signal_(uint8_t logical_slot, const ParsedSignal &signal, bool publish_status);
  bool load_signal_(SignalKind kind, uint8_t logical_slot, StoredSignal &signal, std::string &error);
  void publish_status_(SignalKind kind, const std::string &message);
  void set_last_status_(const std::string &message);
  std::string get_last_status_();
  static std::string json_escape_(const std::string &value);
  static std::string html_escape_(const std::string &value);
  static bool parse_slot_(AsyncWebServerRequest *request, SignalKind kind, uint8_t &slot, std::string &error);
  static uint32_t parse_bounded_u32_(const std::string &value, uint32_t fallback, uint32_t minimum,
                                     uint32_t maximum);
  static std::string format_slot_preview_(uint8_t bit_count, uint64_t code,
                                          const std::vector<int32_t> &timings);
  void refresh_ir_slot_preview_(uint8_t slot);
  void refresh_rf_slot_preview_(uint8_t slot);
  void set_ir_slot_preview_(uint8_t slot, const std::string &preview);
  void set_rf_slot_preview_(uint8_t slot, const std::string &preview);
  bool transmit_ir_now_(const std::vector<int32_t> &timings, uint32_t carrier_hz,
                        uint32_t duty_percent, uint32_t repeats, const char *label);
  bool transmit_rf_now_(const std::vector<int32_t> &timings, uint32_t repeats,
                        uint32_t gap_ms, const char *label);
  static bool parse_hex_code_(const std::string &text, uint64_t &code);

  web_server_base::WebServerBase *base_;
  flash_component::Flash_comp *storage_;
  remote_transmitter::RemoteTransmitterComponent *ir_transmitter_{nullptr};
  remote_transmitter::RemoteTransmitterComponent *rf_transmitter_{nullptr};
  text_sensor::TextSensor *ir_status_{nullptr};
  text_sensor::TextSensor *rf_status_{nullptr};
  std::array<text_sensor::TextSensor *, 10> ir_slot_status_{};
  std::array<text_sensor::TextSensor *, 16> rf_slot_status_{};

  std::string upload_body_;
  bool upload_too_large_{false};
  std::mutex status_mutex_;
  std::string last_status_{"Ready"};
  std::mutex slot_mutex_;
  std::array<std::string, 10> ir_slot_previews_{};
  std::array<std::string, 16> rf_slot_previews_{};
  std::vector<int32_t> last_rf_capture_;
  size_t last_rf_original_count_{0};
  size_t last_rf_clean_count_{0};
};

}  // namespace esphome::flipper_importer
