#include "flipper_importer.h"
#include "flipper_page.h"
#include "esphome/components/remote_base/rc_switch_protocol.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <utility>

namespace esphome::flipper_importer {

[[maybe_unused]] static const char *const TAG = "flipper_importer";



void FlipperImporter::setup() {
  this->base_->add_handler(this);
  this->set_last_status_("Flipper importer ready");
  for (uint8_t slot = 0; slot < 10; slot++) this->refresh_ir_slot_preview_(slot);
  for (uint8_t slot = 0; slot < 16; slot++) this->refresh_rf_slot_preview_(slot);
}

void FlipperImporter::dump_config() {
  ESP_LOGCONFIG(TAG, "Flipper file importer:");
  ESP_LOGCONFIG(TAG, "  Page: /flipper");
  ESP_LOGCONFIG(TAG, "  IR: NEC parsed and raw");
  ESP_LOGCONFIG(TAG, "  RF: Princeton, Dooya and RAW OOK/ASK 433.92 MHz");
}

bool FlipperImporter::canHandle(AsyncWebServerRequest *request) const {
  char url_buf[AsyncWebServerRequest::URL_BUF_SIZE];
  const StringRef url = request->url_to(url_buf);
  if (request->method() == HTTP_GET)
    return url == "/flipper" || url == "/flipper/status" || url == "/flipper/capture" ||
           url == "/flipper/test";
  return request->method() == HTTP_POST && url == "/flipper/import";
}

void FlipperImporter::handleBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
  (void) request;
  if (index == 0) {
    this->upload_body_.clear();
    this->upload_too_large_ = total > MAX_UPLOAD_BYTES;
    if (!this->upload_too_large_) this->upload_body_.reserve(total);
  }
  if (!this->upload_too_large_ && this->upload_body_.size() + len <= MAX_UPLOAD_BYTES)
    this->upload_body_.append(reinterpret_cast<const char *>(data), len);
  else
    this->upload_too_large_ = true;
}

void FlipperImporter::handleUpload(AsyncWebServerRequest *request, const std::string &filename, size_t index,
                                   uint8_t *data, size_t len, bool final) {
  (void) request;
  (void) filename;
  if (index == 0) {
    this->upload_body_.clear();
    this->upload_too_large_ = false;
  }
  if (data != nullptr && len > 0) {
    if (!this->upload_too_large_ && this->upload_body_.size() + len <= MAX_UPLOAD_BYTES)
      this->upload_body_.append(reinterpret_cast<const char *>(data), len);
    else
      this->upload_too_large_ = true;
  }
  if (final && this->upload_too_large_)
    this->upload_body_.clear();
}

void FlipperImporter::handleRequest(AsyncWebServerRequest *request) {
  char url_buf[AsyncWebServerRequest::URL_BUF_SIZE];
  const StringRef url = request->url_to(url_buf);
  if (url == "/flipper") return this->handle_index_(request);
  if (url == "/flipper/status") return this->handle_status_(request);
  if (url == "/flipper/capture") return this->handle_capture_(request);
  if (url == "/flipper/test") return this->handle_test_(request);
  if (url == "/flipper/import") return this->handle_import_(request);
  send_text_response_(request, 404, "text/plain; charset=utf-8", "Not found");
}

void FlipperImporter::handle_index_(AsyncWebServerRequest *request) {
  // Exactly the same response pattern ESPHome uses for its built-in local page:
  // a small pre-compressed static buffer, sent by the standard web_server backend.
  auto *response = request->beginResponse(200, "text/html",
                                          FLIPPER_PAGE_GZ, sizeof(FLIPPER_PAGE_GZ));
  response->addHeader("Content-Encoding", "gzip");
  response->addHeader("Cache-Control", "no-store, no-cache, must-revalidate");
  request->send(response);
}

void FlipperImporter::send_text_response_(AsyncWebServerRequest *request, int code, const char *content_type,
                                           const std::string &body) {
  auto *response = request->beginResponse(code, content_type, body);
  response->addHeader("Cache-Control", "no-store, no-cache, must-revalidate");
  response->addHeader("Pragma", "no-cache");
  request->send(response);
}

void FlipperImporter::send_ui_result_(AsyncWebServerRequest *request, int code, const std::string &message) {
  const std::string body = "<!doctype html><meta charset=utf-8><title>Flipper</title><p>" +
                           html_escape_(message) + "</p><p><a href=/flipper>Back</a></p>";
  send_text_response_(request, code, "text/html; charset=utf-8", body);
}

void FlipperImporter::handle_status_(AsyncWebServerRequest *request) {
  std::string message;
  size_t raw_count = 0;
  size_t clean_count = 0;
  {
    std::lock_guard<std::mutex> lock(this->status_mutex_);
    message = this->last_status_;
    raw_count = this->last_rf_original_count_;
    clean_count = this->last_rf_clean_count_;
  }
  const std::string body = "{\"message\":\"" + json_escape_(message) +
                           "\",\"rf_capture_count\":" + std::to_string(raw_count) +
                           ",\"rf_clean_count\":" + std::to_string(clean_count) + "}";
  send_text_response_(request, 200, "application/json; charset=utf-8", body);
}

void FlipperImporter::handle_capture_(AsyncWebServerRequest *request) {
  std::vector<int32_t> capture;
  {
    std::lock_guard<std::mutex> lock(this->status_mutex_);
    capture = this->last_rf_capture_;
  }
  if (capture.empty()) {
    send_text_response_(request, 404, "text/plain; charset=utf-8", "No RF capture is available yet.");
    return;
  }
  std::string body = "Filetype: Flipper SubGhz RAW File\nVersion: 1\nFrequency: 433920000\n"
                     "Preset: FuriHalSubGhzPresetOok650Async\nProtocol: RAW\nRAW_Data:";
  body.reserve(body.size() + capture.size() * 8);
  for (const int32_t value : capture) body += " " + std::to_string(value);
  body += "\n";
  auto *response = request->beginResponse(200, "text/plain; charset=utf-8", body);
  response->addHeader("Content-Disposition", "attachment; filename=AR01V3-last-RF-capture.sub");
  response->addHeader("Cache-Control", "no-store, no-cache, must-revalidate");
  request->send(response);
}

bool FlipperImporter::parse_slot_(AsyncWebServerRequest *request, SignalKind kind, uint8_t &slot, std::string &error) {
  if (!request->hasArg("slot")) {
    error = "Missing slot parameter";
    return false;
  }
  char *end = nullptr;
  const long parsed = std::strtol(request->arg("slot").c_str(), &end, 10);
  const long maximum = kind == SignalKind::IR ? 9 : 15;
  if (end == nullptr || *end != '\0' || parsed < 0 || parsed > maximum) {
    error = kind == SignalKind::IR ? "IR slot must be in range 0..9" : "RF slot must be in range 0..15";
    return false;
  }
  slot = static_cast<uint8_t>(parsed);
  return true;
}

void FlipperImporter::handle_import_(AsyncWebServerRequest *request) {
  if (this->upload_too_large_) {
    this->upload_body_.clear();
    if (request->arg("ui") == "1") send_ui_result_(request, 400, "File exceeds 24 KiB");
    else send_text_response_(request, 400, "application/json; charset=utf-8",
                             "{\"message\":\"File exceeds 24 KiB\"}");
    return;
  }

  ParsedSignal parsed;
  std::string error;
  if (!parse_flipper_file(this->upload_body_, parsed, error)) {
    this->upload_body_.clear();
    const std::string body = "{\"message\":\"Import failed: " + json_escape_(error) + "\"}";
    this->set_last_status_("Import failed: " + error);
    if (request->arg("ui") == "1") send_ui_result_(request, 422, "Import failed: " + error);
    else send_text_response_(request, 422, "application/json; charset=utf-8", body);
    return;
  }
  this->upload_body_.clear();

  const std::string requested_kind = lower_copy(request->arg("kind"));
  if ((requested_kind == "ir" && parsed.kind != SignalKind::IR) ||
      (requested_kind == "rf" && parsed.kind != SignalKind::RF) ||
      (requested_kind != "ir" && requested_kind != "rf")) {
    if (request->arg("ui") == "1") send_ui_result_(request, 422, "The selected signal type does not match the file");
    else send_text_response_(request, 422, "application/json; charset=utf-8",
                             "{\"message\":\"The selected signal type does not match the file\"}");
    return;
  }

  uint8_t slot = 0;
  if (!parse_slot_(request, parsed.kind, slot, error)) {
    const std::string body = "{\"message\":\"" + json_escape_(error) + "\"}";
    if (request->arg("ui") == "1") send_ui_result_(request, 400, error);
    else send_text_response_(request, 400, "application/json; charset=utf-8", body);
    return;
  }

  const std::string details = parsed.source_format + ", " + std::to_string(parsed.timings.size()) +
                              " timings, " + std::to_string(parsed.frequency) + " Hz";
  const std::string queued = "Import queued: " + parsed.name + " -> " +
                             (parsed.kind == SignalKind::IR ? "IR " : "RF ") + std::to_string(slot);
  this->set_last_status_(queued);
  const bool test_after_import = request->arg("test") == "1";
  const ParsedSignal parsed_copy = parsed;
  this->defer([this, slot, parsed_copy, test_after_import]() {
    const bool ok = this->save_signal_(slot, parsed_copy, true);
    if (!ok) this->publish_status_(parsed_copy.kind, "NVS write failed");
    else if (test_after_import) {
      if (parsed_copy.kind == SignalKind::IR) this->send_ir_slot(slot);
      else this->send_rf_slot(slot, 0, 0);
    }
  });

  const std::string body = "{\"message\":\"" + json_escape_(queued) +
                           "\",\"details\":\"" + json_escape_(details) +
                           "\",\"recommended_repeats\":" + std::to_string(parsed.recommended_repeats) + "}";
  if (request->arg("ui") == "1") send_ui_result_(request, 200, queued + ". " + details);
  else send_text_response_(request, 200, "application/json; charset=utf-8", body);
}

uint32_t FlipperImporter::parse_bounded_u32_(const std::string &value, uint32_t fallback, uint32_t minimum,
                                              uint32_t maximum) {
  if (value.empty()) return fallback;
  char *end = nullptr;
  const unsigned long parsed = std::strtoul(value.c_str(), &end, 10);
  if (end == nullptr || *end != '\0') return fallback;
  return std::clamp<uint32_t>(static_cast<uint32_t>(parsed), minimum, maximum);
}

void FlipperImporter::handle_test_(AsyncWebServerRequest *request) {
  const std::string kind_text = lower_copy(request->arg("kind"));
  SignalKind kind;
  if (kind_text == "ir") kind = SignalKind::IR;
  else if (kind_text == "rf") kind = SignalKind::RF;
  else {
    if (request->arg("ui") == "1") send_ui_result_(request, 400, "Invalid signal type");
    else send_text_response_(request, 400, "application/json; charset=utf-8",
                             "{\"message\":\"Invalid signal type\"}");
    return;
  }
  uint8_t slot = 0;
  std::string error;
  if (!parse_slot_(request, kind, slot, error)) {
    const std::string body = "{\"message\":\"" + json_escape_(error) + "\"}";
    if (request->arg("ui") == "1") send_ui_result_(request, 400, error);
    else send_text_response_(request, 400, "application/json; charset=utf-8", body);
    return;
  }
  const uint32_t repeats = parse_bounded_u32_(request->arg("repeats"), 5, 1, 20);
  const uint32_t gap_ms = parse_bounded_u32_(request->arg("gap"), 0, 0, 100);
  this->defer([this, kind, slot, repeats, gap_ms]() {
    if (kind == SignalKind::IR) this->send_ir_slot(slot);
    else this->send_rf_slot(slot, repeats, gap_ms * 1000U);
  });
  const std::string message = std::string("Test queued: ") + (kind == SignalKind::IR ? "IR " : "RF ") +
                              std::to_string(slot);
  this->set_last_status_(message);
  const std::string body = "{\"message\":\"" + json_escape_(message) + "\"}";
  if (request->arg("ui") == "1") send_ui_result_(request, 200, message);
  else send_text_response_(request, 200, "application/json; charset=utf-8", body);
}

bool FlipperImporter::save_signal_(uint8_t logical_slot, const ParsedSignal &signal, bool publish_status) {
  const uint8_t maximum = signal.kind == SignalKind::IR ? 9 : 15;
  if (logical_slot > maximum) return false;
  std::string timing_error;
  const bool allow_leading_space =
      (signal.encoding == SignalEncoding::RC_SWITCH && signal.protocol == 6) ||
      signal.encoding == SignalEncoding::DOOYA;
  if (!validate_signed_timings(signal.timings, timing_error, allow_leading_space)) {
    if (publish_status) this->publish_status_(signal.kind, "Invalid signal: " + timing_error);
    return false;
  }
  const int nvs_index = signal.kind == SignalKind::IR ? logical_slot : logical_slot + 10;
  const std::vector<int32_t> record = encode_record(signal);
  const bool ok = this->storage_->save_to_nvs<int32_t>(nvs_index, record);
  if (ok) {
    const std::string preview = this->format_slot_preview_(signal.bit_count, signal.code, signal.timings);
    if (signal.kind == SignalKind::IR) this->set_ir_slot_preview_(logical_slot, preview);
    else this->set_rf_slot_preview_(logical_slot, preview);
  }
  if (publish_status) {
    const std::string message = ok ? (std::string("Saved ") + signal.name + " to slot " +
                                      std::to_string(logical_slot))
                                   : "NVS write failed";
    this->publish_status_(signal.kind, message);
  }
  return ok;
}

bool FlipperImporter::save_ir_raw(uint8_t slot, const std::vector<int32_t> &timings, uint32_t frequency,
                                  uint8_t duty_percent, uint8_t recommended_repeats) {
  ParsedSignal signal;
  signal.kind = SignalKind::IR;
  signal.name = "learned IR";
  signal.source_format = "AR01V3 IR RAW";
  signal.frequency = frequency;
  signal.duty_percent = std::clamp<uint8_t>(duty_percent, 1, 100);
  signal.recommended_repeats = std::clamp<uint8_t>(recommended_repeats, 1, 20);
  signal.encoding = SignalEncoding::RAW;
  signal.timings = timings;
  std::string normalize_error;
  if (!normalize_signed_timings(signal.timings, normalize_error)) return false;
  // The configured IR receiver closes a capture with an artificial idle SPACE.
  // It is not part of the command and would make replay appear unnecessarily slow.
  if (!signal.timings.empty() && signal.timings.back() < -50000) signal.timings.pop_back();
  return this->save_signal_(slot, signal, false);
}

bool FlipperImporter::save_ir_nec(uint8_t slot, uint16_t address, uint16_t command) {
  ParsedSignal signal;
  signal.kind = SignalKind::IR;
  signal.name = "learned NEC";
  signal.source_format = "AR01V3 NEC decoded";
  signal.frequency = 38000;
  signal.duty_percent = 33;
  signal.recommended_repeats = 1;
  signal.encoding = SignalEncoding::NEC;
  signal.bit_count = 32;
  signal.pulse_length = 560;
  signal.code = (static_cast<uint64_t>(address) << 16U) | command;
  signal.timings = build_nec_timings(address, command);
  return this->save_signal_(slot, signal, false);
}

bool FlipperImporter::save_rf_raw(uint8_t slot, const std::vector<int32_t> &timings, uint32_t frequency,
                                  uint8_t recommended_repeats) {
  ParsedSignal signal;
  signal.kind = SignalKind::RF;
  signal.name = "learned RF";
  signal.source_format = "AR01V3 RF RAW";
  signal.frequency = frequency;
  signal.duty_percent = 100;
  signal.recommended_repeats = std::clamp<uint8_t>(recommended_repeats, 1, 20);
  signal.encoding = SignalEncoding::RAW;
  signal.timings = timings;
  return this->save_signal_(slot, signal, false);
}

bool FlipperImporter::save_rf_rc_switch(uint8_t slot, uint64_t code, uint8_t protocol,
                                        uint8_t recommended_repeats) {
  if (slot > 15 || protocol < 1 || protocol > 8) return false;

  ParsedSignal signal;
  signal.kind = SignalKind::RF;
  signal.frequency = 433920000;
  signal.duty_percent = 100;
  signal.recommended_repeats = std::clamp<uint8_t>(recommended_repeats, 1, 20);
  signal.bit_count = 24;
  signal.code = code;

  if (protocol == 6) {
    // On this AR01V3 the working Princeton/protocol-1 TX waveform is reported
    // by the inverted GPIO19 receive path as protocol 6. Re-emitting protocol 6
    // produced a different 23-bit waveform. Map it back to the proven Flipper
    // Princeton representation instead of copying the RX decoder number to TX.
    signal.name = "learned Princeton";
    signal.source_format = "AR01V3 RX protocol 6 mapped to Princeton TX";
    signal.encoding = SignalEncoding::PRINCETON;
    signal.protocol = 1;
    signal.pulse_length = 403;
    signal.guard_multiplier = 30;
    signal.timings = build_princeton_timings(code, 24, 403, 30);
  } else {
    signal.name = "learned RC-Switch";
    signal.source_format = "Athom AR01V3 rc_switch";
    signal.encoding = SignalEncoding::RC_SWITCH;
    signal.protocol = protocol;
    remote_base::RemoteTransmitData encoded;
    remote_base::RC_SWITCH_PROTOCOLS[protocol].transmit(&encoded, code, 24);
    signal.timings = encoded.get_data();
  }
  return this->save_signal_(slot, signal, false);
}

bool FlipperImporter::save_rf_captured_raw(uint8_t slot, const std::vector<int32_t> &timings,
                                           uint32_t frequency) {
  ParsedSignal signal;
  signal.kind = SignalKind::RF;
  signal.name = "captured RF burst";
  signal.source_format = "AR01V3 exact RF capture";
  signal.frequency = frequency;
  signal.duty_percent = 100;
  signal.recommended_repeats = 1;
  signal.encoding = SignalEncoding::CAPTURED_RAW;
  signal.timings = timings;

  // This is lossless waveform canonicalization only: discard silence before
  // the first mark and merge adjacent durations with the same level. Do not
  // deglitch, decode, average, align or cut the captured burst.
  std::string error;
  if (!normalize_signed_timings(signal.timings, error)) return false;
  return this->save_signal_(slot, signal, false);
}

int FlipperImporter::learn_rf_capture(uint8_t slot, const std::vector<int32_t> &capture, bool has_rc_switch,
                                      uint64_t rc_code, uint8_t rc_protocol, uint8_t recommended_repeats,
                                      bool fast_raw, std::string &message) {
  if (slot > 15 || capture.size() < 16 || capture.size() > 8192) {
    message = "Invalid RF capture";
    return -1;
  }

  const unsigned original_count = static_cast<unsigned>(capture.size());
  {
    std::lock_guard<std::mutex> lock(this->status_mutex_);
    this->last_rf_original_count_ = capture.size();
    this->last_rf_capture_ = capture;
    this->last_rf_clean_count_ = capture.size();
  }

  // Exact RAW is deliberately handled before every decoder and heuristic.
  // The complete RMT burst is saved and later transmitted once. It is not a
  // fallback for a failed protocol decoder; it is a separate tape-recorder path.
  if (fast_raw) {
    std::vector<int32_t> exact_capture = capture;
    std::string timing_error;
    if (!normalize_signed_timings(exact_capture, timing_error) ||
        !validate_signed_timings(exact_capture, timing_error)) {
      message = "Exact RAW cannot be saved: " + timing_error;
      return -1;
    }
    if (!this->save_rf_captured_raw(slot, exact_capture, 433920000)) {
      message = "Exact RAW NVS write failed";
      return -1;
    }
    char status[128];
    std::snprintf(status, sizeof(status),
                  "Saved exact RAW burst (%u timings); it will be replayed once",
                  (unsigned) exact_capture.size());
    message = status;
    return 1;
  }

  std::vector<int32_t> cleaned_capture = capture;
  deglitch_rf_timings(cleaned_capture);
  const unsigned cleaned_count = static_cast<unsigned>(cleaned_capture.size());
  {
    std::lock_guard<std::mutex> lock(this->status_mutex_);
    this->last_rf_clean_count_ = cleaned_capture.size();
  }

  // Protocol-first means protocol-first: do not reject a valid rc_switch /
  // Princeton decode merely because the repeated-RAW heuristic failed.
  // Decode independently. With the deliberately wider receiver tolerance an
  // RC-Switch callback is useful confirmation, but a partial/false callback
  // must not veto a complete Princeton frame recovered from the measured RAW.
  PrincetonFrame princeton = decode_princeton_capture(cleaned_capture, false, 0);
  if (princeton.valid) {
    const bool rc_confirmed = has_rc_switch && rc_protocol == 1 && rc_code == princeton.code;
    ParsedSignal signal;
    signal.kind = SignalKind::RF;
    signal.name = "learned Princeton";
    signal.source_format = "AR01V3 Princeton decoded";
    signal.frequency = 433920000;
    signal.duty_percent = 100;
    signal.recommended_repeats = std::clamp<uint8_t>(recommended_repeats, 1, 20);
    signal.encoding = SignalEncoding::PRINCETON;
    signal.protocol = 1;
    signal.bit_count = princeton.bit_count;
    signal.pulse_length = princeton.pulse_length;
    signal.guard_multiplier = princeton.guard_multiplier;
    signal.code = princeton.code;
    signal.timings = princeton.timings;
    if (!this->save_signal_(slot, signal, false)) {
      message = "NVS write failed";
      return -1;
    }
    char status[160];
    std::snprintf(status, sizeof(status),
                  "Princeton%s: %u-bit, TE %u us, code 0x%llX, %u frame(s), offset %u [%u->%u]",
                  rc_confirmed ? " (RC-Switch)" : "",
                  princeton.bit_count, (unsigned) princeton.pulse_length,
                  (unsigned long long) princeton.code, (unsigned) princeton.matching_frames,
                  (unsigned) princeton.capture_offset, original_count, cleaned_count);
    message = status;
    return 1;
  }

  char status[160];
  if (has_rc_switch)
    std::snprintf(status, sizeof(status),
                  "RC-Switch %u callback was not confirmed by the protocol decoder [%u->%u]",
                  rc_protocol, original_count, cleaned_count);
  else
    std::snprintf(status, sizeof(status),
                  "Protocol not decoded [%u->%u]; use Exact RAW or download the capture",
                  original_count, cleaned_count);
  message = status;
  return -1;
}

bool FlipperImporter::clear_ir_slot(uint8_t slot) {
  if (slot > 9) return false;
  const bool ok = this->storage_->clear_signal_by_index(slot);
  if (ok) this->set_ir_slot_preview_(slot, "None");
  this->publish_status_(SignalKind::IR, ok ? "IR slot cleared" : "Could not clear IR slot");
  return ok;
}

bool FlipperImporter::clear_rf_slot(uint8_t slot) {
  if (slot > 15) return false;
  const bool ok = this->storage_->clear_signal_by_index(slot + 10);
  if (ok) this->set_rf_slot_preview_(slot, "None");
  this->publish_status_(SignalKind::RF, ok ? "RF slot cleared" : "Could not clear RF slot");
  return ok;
}

std::string FlipperImporter::format_slot_preview_(uint8_t bit_count, uint64_t code,
                                                  const std::vector<int32_t> &timings) {
  char buffer[32];
  if (bit_count > 0) {
    const unsigned byte_count = std::clamp<unsigned>((bit_count + 7U) / 8U, 1U, 8U);
    std::snprintf(buffer, sizeof(buffer), "0x%0*llX", static_cast<int>(byte_count * 2U),
                  static_cast<unsigned long long>(code));
    std::string preview(buffer);
    if (bit_count > 64) preview += "...";
    return preview;
  }

  // RAW has no protocol code. Show the first eight bytes of its signed int32
  // timing payload in deterministic big-endian order, then mark truncation.
  std::string preview{"0x"};
  unsigned emitted = 0;
  for (int32_t timing : timings) {
    const uint32_t word = static_cast<uint32_t>(timing);
    for (int shift = 24; shift >= 0 && emitted < 8; shift -= 8, emitted++) {
      const auto byte = static_cast<unsigned int>((word >> shift) & UINT64_C(0xFF));
      std::snprintf(buffer, sizeof(buffer), "%02X", byte);
      preview += buffer;
    }
    if (emitted == 8) break;
  }
  if (emitted == 0) return "None";
  if (timings.size() * sizeof(int32_t) > 8) preview += "...";
  return preview;
}

void FlipperImporter::set_ir_slot_preview_(uint8_t slot, const std::string &preview) {
  if (slot > 9) return;
  {
    std::lock_guard<std::mutex> lock(this->slot_mutex_);
    this->ir_slot_previews_[slot] = preview;
  }
  if (this->ir_slot_status_[slot] != nullptr) this->ir_slot_status_[slot]->publish_state(preview);
}

void FlipperImporter::set_rf_slot_preview_(uint8_t slot, const std::string &preview) {
  if (slot > 15) return;
  {
    std::lock_guard<std::mutex> lock(this->slot_mutex_);
    this->rf_slot_previews_[slot] = preview;
  }
  if (this->rf_slot_status_[slot] != nullptr) this->rf_slot_status_[slot]->publish_state(preview);
}

void FlipperImporter::refresh_ir_slot_preview_(uint8_t slot) {
  StoredSignal signal;
  std::string error;
  if (this->load_signal_(SignalKind::IR, slot, signal, error))
    this->set_ir_slot_preview_(slot,
                               this->format_slot_preview_(signal.bit_count, signal.code, signal.timings));
  else
    this->set_ir_slot_preview_(slot, "None");
}

void FlipperImporter::refresh_rf_slot_preview_(uint8_t slot) {
  StoredSignal signal;
  std::string error;
  if (this->load_signal_(SignalKind::RF, slot, signal, error))
    this->set_rf_slot_preview_(slot,
                               this->format_slot_preview_(signal.bit_count, signal.code, signal.timings));
  else
    this->set_rf_slot_preview_(slot, "None");
}

std::string FlipperImporter::ir_slot_preview(uint8_t slot) {
  if (slot > 9) return "None";
  std::lock_guard<std::mutex> lock(this->slot_mutex_);
  return this->ir_slot_previews_[slot].empty() ? "None" : this->ir_slot_previews_[slot];
}

std::string FlipperImporter::rf_slot_preview(uint8_t slot) {
  if (slot > 15) return "None";
  std::lock_guard<std::mutex> lock(this->slot_mutex_);
  return this->rf_slot_previews_[slot].empty() ? "None" : this->rf_slot_previews_[slot];
}

bool FlipperImporter::load_signal_(SignalKind kind, uint8_t logical_slot, StoredSignal &signal, std::string &error) {
  const uint8_t maximum = kind == SignalKind::IR ? 9 : 15;
  if (logical_slot > maximum) {
    error = "Invalid slot";
    return false;
  }
  const int nvs_index = kind == SignalKind::IR ? logical_slot : logical_slot + 10;
  const std::vector<int32_t> record = this->storage_->load_from_nvs<int32_t>(nvs_index);
  if (decode_record(record, signal, &error)) {
    if (signal.kind != kind) {
      error = "Slot contains a different signal type";
      return false;
    }
    return true;
  }

  // Backward compatibility: legacy IR records stored a bare std::vector<int32_t> without a header.
  if (kind == SignalKind::IR && !record.empty()) {
    std::string timing_error;
    if (validate_signed_timings(record, timing_error)) {
      signal.kind = SignalKind::IR;
      signal.frequency = 38000;
      signal.duty_percent = 50;
      signal.recommended_repeats = 1;
      signal.timings = record;
      return true;
    }
  }

  // Backward compatibility with RF RAW v1: int16_t [30001,-30001,1,count,...].
  if (kind == SignalKind::RF) {
    const std::vector<int16_t> legacy = this->storage_->load_from_nvs<int16_t>(nvs_index);
    if (legacy.size() >= 5 && legacy[0] == 30001 && legacy[1] == -30001 && legacy[2] == 1) {
      const int count = legacy[3];
      if (count >= 1 && count <= static_cast<int>(MAX_SIGNAL_TIMINGS) &&
          legacy.size() == static_cast<size_t>(count + 4)) {
        signal.kind = SignalKind::RF;
        signal.frequency = 433920000;
        signal.duty_percent = 100;
        signal.recommended_repeats = 10;
        signal.timings.reserve(count);
        for (int i = 0; i < count; i++) signal.timings.push_back(legacy[i + 4]);
        std::string timing_error;
        if (validate_signed_timings(signal.timings, timing_error)) return true;
      }
    }
  }
  error = "Slot is empty or contains an invalid record";
  return false;
}

bool FlipperImporter::send_ir_slot(uint8_t slot) {
  StoredSignal signal;
  std::string error;
  if (this->ir_transmitter_ == nullptr || !this->load_signal_(SignalKind::IR, slot, signal, error)) {
    this->publish_status_(SignalKind::IR, "IR was not sent: " + (this->ir_transmitter_ == nullptr ?
                                                                  std::string("transmitter unavailable") : error));
    return false;
  }
  this->ir_transmitter_->set_carrier_duty_percent(signal.duty_percent);
  auto call = this->ir_transmitter_->transmit();
  call.get_data()->set_carrier_frequency(signal.frequency == 0 ? 38000 : signal.frequency);
  call.get_data()->set_data(signal.timings);
  call.set_send_times(std::clamp<uint32_t>(signal.recommended_repeats, 1, 20));
  call.perform();
  const std::string message = "Sent IR slot " + std::to_string(slot) + " (" +
                              std::to_string(signal.timings.size()) + " timings)";
  this->publish_status_(SignalKind::IR, message);
  return true;
}

bool FlipperImporter::send_rf_slot(uint8_t slot, uint32_t repeats, uint32_t wait_us) {
  StoredSignal signal;
  std::string error;
  if (this->rf_transmitter_ == nullptr || !this->load_signal_(SignalKind::RF, slot, signal, error)) {
    this->publish_status_(SignalKind::RF, "RF was not sent: " + (this->rf_transmitter_ == nullptr ?
                                                                  std::string("transmitter unavailable") : error));
    return false;
  }
  this->rf_transmitter_->set_carrier_duty_percent(100);
  auto call = this->rf_transmitter_->transmit();
  call.get_data()->set_carrier_frequency(0);
  call.get_data()->set_data(signal.timings);
  const bool captured_burst = signal.encoding == SignalEncoding::CAPTURED_RAW;
  const uint32_t actual_repeats = captured_burst ? 1U :
      (repeats == 0 ? signal.recommended_repeats : std::clamp<uint32_t>(repeats, 1, 20));
  call.set_send_times(actual_repeats);
  call.set_send_wait(captured_burst ? 0U : std::min<uint32_t>(wait_us, 100000));
  call.perform();
  const std::string message = "Sent RF slot " + std::to_string(slot) + " x" +
                              std::to_string(actual_repeats);
  this->publish_status_(SignalKind::RF, message);
  return true;
}

bool FlipperImporter::transmit_ir_now_(const std::vector<int32_t> &timings, uint32_t carrier_hz,
                                       uint32_t duty_percent, uint32_t repeats, const char *label) {
  std::string error;
  if (this->ir_transmitter_ == nullptr) {
    this->publish_status_(SignalKind::IR, "Dynamic IR was not sent: transmitter unavailable");
    return false;
  }
  if (!validate_signed_timings(timings, error) || carrier_hz < 20000 || carrier_hz > 60000 ||
      duty_percent < 1 || duty_percent > 100 || repeats < 1 || repeats > 20) {
    if (error.empty()) error = "carrier must be 20000..60000 Hz, duty 1..100 and repeats 1..20";
    this->publish_status_(SignalKind::IR, "Dynamic IR was not sent: " + error);
    return false;
  }

  this->ir_transmitter_->set_carrier_duty_percent(static_cast<uint8_t>(duty_percent));
  auto call = this->ir_transmitter_->transmit();
  call.get_data()->set_carrier_frequency(carrier_hz);
  call.get_data()->set_data(timings);
  call.set_send_times(repeats);
  call.perform();
  this->publish_status_(SignalKind::IR, std::string("Sent dynamic IR ") + label + " (" +
                                               std::to_string(timings.size()) + " timings) x" +
                                               std::to_string(repeats));
  return true;
}

bool FlipperImporter::transmit_rf_now_(const std::vector<int32_t> &timings, uint32_t repeats,
                                       uint32_t gap_ms, const char *label) {
  std::string error;
  if (this->rf_transmitter_ == nullptr) {
    this->publish_status_(SignalKind::RF, "Dynamic RF was not sent: transmitter unavailable");
    return false;
  }
  if (!validate_signed_timings(timings, error, true) || repeats < 1 || repeats > 20 || gap_ms > 100) {
    if (error.empty()) error = "repeats must be 1..20 and gap_ms 0..100";
    this->publish_status_(SignalKind::RF, "Dynamic RF was not sent: " + error);
    return false;
  }

  this->rf_transmitter_->set_carrier_duty_percent(100);
  auto call = this->rf_transmitter_->transmit();
  call.get_data()->set_carrier_frequency(0);
  call.get_data()->set_data(timings);
  call.set_send_times(repeats);
  call.set_send_wait(gap_ms * 1000U);
  call.perform();
  this->publish_status_(SignalKind::RF, std::string("Sent dynamic RF ") + label + " (" +
                                               std::to_string(timings.size()) + " timings) x" +
                                               std::to_string(repeats));
  return true;
}

bool FlipperImporter::parse_hex_code_(const std::string &text, uint64_t &code) {
  const std::string value = trim_copy(text);
  return !value.empty() && value.front() != '-' && parse_u64(value, code, 16);
}

bool FlipperImporter::send_ir_raw_now(const std::vector<int32_t> &timings, uint32_t carrier_hz,
                                      uint32_t duty_percent, uint32_t repeats) {
  return this->transmit_ir_now_(timings, carrier_hz, duty_percent, repeats, "RAW");
}

bool FlipperImporter::send_ir_raw_text_now(const std::string &timings, uint32_t carrier_hz,
                                           uint32_t duty_percent, uint32_t repeats) {
  std::vector<int32_t> parsed;
  std::string error;
  if (!parse_signed_timings_text(timings, parsed, error)) {
    this->publish_status_(SignalKind::IR, "Dynamic IR RAW was not sent: " + error);
    return false;
  }
  return this->send_ir_raw_now(parsed, carrier_hz, duty_percent, repeats);
}

bool FlipperImporter::send_ir_nec_now(uint32_t address, uint32_t command, uint32_t repeats) {
  if (address > 0xFFU || command > 0xFFU) {
    this->publish_status_(SignalKind::IR, "Dynamic NEC was not sent: address and command must be 0..255");
    return false;
  }
  const uint16_t encoded_address = static_cast<uint16_t>(address | ((~address & 0xFFU) << 8U));
  const uint16_t encoded_command = static_cast<uint16_t>(command | ((~command & 0xFFU) << 8U));
  return this->transmit_ir_now_(build_nec_timings(encoded_address, encoded_command), 38000, 33,
                                repeats, "NEC");
}

bool FlipperImporter::send_rf_raw_now(const std::vector<int32_t> &timings, uint32_t repeats,
                                      uint32_t gap_ms) {
  return this->transmit_rf_now_(timings, repeats, gap_ms, "RAW");
}

bool FlipperImporter::send_rf_raw_text_now(const std::string &timings, uint32_t repeats,
                                           uint32_t gap_ms) {
  std::vector<int32_t> parsed;
  std::string error;
  if (!parse_signed_timings_text(timings, parsed, error)) {
    this->publish_status_(SignalKind::RF, "Dynamic RF RAW was not sent: " + error);
    return false;
  }
  return this->send_rf_raw_now(parsed, repeats, gap_ms);
}

bool FlipperImporter::send_rf_princeton_now(const std::string &code_hex, uint32_t bit_count,
                                            uint32_t te_us, uint32_t guard_multiplier,
                                            uint32_t repeats, uint32_t gap_ms) {
  uint64_t code = 0;
  if (!parse_hex_code_(code_hex, code) || bit_count < 1 || bit_count > 64 ||
      (bit_count < 64 && (code >> bit_count) != 0) || te_us < 50 || te_us > 5000 ||
      guard_multiplier < 15 || guard_multiplier > 72) {
    this->publish_status_(SignalKind::RF,
                          "Dynamic Princeton was not sent: invalid code, bits, TE or guard");
    return false;
  }
  return this->transmit_rf_now_(
      build_princeton_timings(code, static_cast<uint8_t>(bit_count), te_us, guard_multiplier),
      repeats, gap_ms, "Princeton");
}

bool FlipperImporter::send_rf_dooya_now(const std::string &code_hex, uint32_t repeats) {
  uint64_t code = 0;
  if (!parse_hex_code_(code_hex, code) || (code >> 40U) != 0) {
    this->publish_status_(SignalKind::RF, "Dynamic Dooya was not sent: code must contain at most 40 bits");
    return false;
  }
  return this->transmit_rf_now_(build_dooya_timings(code), repeats, 0, "Dooya");
}

void FlipperImporter::publish_status_(SignalKind kind, const std::string &message) {
  this->set_last_status_(message);
  if (kind == SignalKind::IR && this->ir_status_ != nullptr) this->ir_status_->publish_state(message);
  if (kind == SignalKind::RF && this->rf_status_ != nullptr) this->rf_status_->publish_state(message);
  ESP_LOGI(TAG, "%s", message.c_str());
}

void FlipperImporter::set_last_status_(const std::string &message) {
  std::lock_guard<std::mutex> lock(this->status_mutex_);
  this->last_status_ = message;
}

std::string FlipperImporter::get_last_status_() {
  std::lock_guard<std::mutex> lock(this->status_mutex_);
  return this->last_status_;
}

std::string FlipperImporter::json_escape_(const std::string &value) {
  std::string escaped;
  escaped.reserve(value.size() + 8);
  for (const char c : value) {
    switch (c) {
      case '\\': escaped += "\\\\"; break;
      case '"': escaped += "\\\""; break;
      case '\n': escaped += "\\n"; break;
      case '\r': escaped += "\\r"; break;
      case '\t': escaped += "\\t"; break;
      default:
        if (static_cast<unsigned char>(c) >= 0x20) escaped.push_back(c);
        break;
    }
  }
  return escaped;
}

std::string FlipperImporter::html_escape_(const std::string &value) {
  std::string out;
  out.reserve(value.size());
  for (const char ch : value) {
    switch (ch) {
      case '&': out += "&amp;"; break;
      case '<': out += "&lt;"; break;
      case '>': out += "&gt;"; break;
      case '"': out += "&quot;"; break;
      default: out.push_back(ch); break;
    }
  }
  return out;
}

}  // namespace esphome::flipper_importer
