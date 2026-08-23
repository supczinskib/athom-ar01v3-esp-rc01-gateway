#pragma once

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace esphome::flipper_importer {

enum class SignalKind : int32_t { IR = 1, RF = 2 };

enum class SignalEncoding : int32_t {
  RAW = 0,
  NEC = 1,
  PRINCETON = 2,
  RC_SWITCH = 3,
  // A complete burst captured by the AR01V3 receiver. Unlike imported RAW,
  // this already contains the transmitter's repetitions and is replayed once.
  CAPTURED_RAW = 4,
  DOOYA = 5,
};

struct ParsedSignal {
  SignalKind kind{SignalKind::IR};
  std::string name;
  std::string source_format;
  uint32_t frequency{0};
  uint8_t duty_percent{0};
  uint8_t recommended_repeats{1};
  SignalEncoding encoding{SignalEncoding::RAW};
  uint8_t protocol{0};
  uint8_t bit_count{0};
  uint32_t pulse_length{0};
  uint32_t guard_multiplier{0};
  uint64_t code{0};
  std::vector<int32_t> timings;
};

struct StoredSignal {
  SignalKind kind{SignalKind::IR};
  uint32_t frequency{0};
  uint8_t duty_percent{0};
  uint8_t recommended_repeats{1};
  SignalEncoding encoding{SignalEncoding::RAW};
  uint8_t protocol{0};
  uint8_t bit_count{0};
  uint32_t pulse_length{0};
  uint32_t guard_multiplier{0};
  uint64_t code{0};
  std::vector<int32_t> timings;
};

inline constexpr int32_t SIGNAL_MAGIC_1 = 0x41523031;  // "AR01"
inline constexpr int32_t SIGNAL_MAGIC_2 = 0x53494732;  // "SIG2"
inline constexpr int32_t SIGNAL_VERSION = 3;
inline constexpr size_t SIGNAL_V2_HEADER_WORDS = 8;
inline constexpr size_t SIGNAL_HEADER_WORDS = 15;
inline constexpr size_t MAX_SIGNAL_TIMINGS = 2048;
inline constexpr size_t MAX_FLIPPER_FILE_BYTES = 12 * 1024;

inline std::string trim_copy(const std::string &input) {
  const auto first = input.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) return {};
  const auto last = input.find_last_not_of(" \t\r\n");
  return input.substr(first, last - first + 1);
}

inline std::string lower_copy(std::string input) {
  std::transform(input.begin(), input.end(), input.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return input;
}

inline bool starts_with_ci(const std::string &value, const std::string &prefix) {
  if (value.size() < prefix.size()) return false;
  for (size_t i = 0; i < prefix.size(); i++) {
    if (std::tolower(static_cast<unsigned char>(value[i])) !=
        std::tolower(static_cast<unsigned char>(prefix[i])))
      return false;
  }
  return true;
}

inline bool parse_u64(const std::string &text, uint64_t &value, int base = 10) {
  const std::string trimmed = trim_copy(text);
  if (trimmed.empty()) return false;
  errno = 0;
  char *end = nullptr;
  const unsigned long long parsed = std::strtoull(trimmed.c_str(), &end, base);
  if (errno != 0 || end == trimmed.c_str() || *end != '\0') return false;
  value = static_cast<uint64_t>(parsed);
  return true;
}

inline bool parse_i32(const std::string &text, int32_t &value) {
  const std::string trimmed = trim_copy(text);
  if (trimmed.empty()) return false;
  errno = 0;
  char *end = nullptr;
  const long long parsed = std::strtoll(trimmed.c_str(), &end, 10);
  if (errno != 0 || end == trimmed.c_str() || *end != '\0' ||
      parsed < std::numeric_limits<int32_t>::min() || parsed > std::numeric_limits<int32_t>::max())
    return false;
  value = static_cast<int32_t>(parsed);
  return true;
}

// ESPHome 2026.7 represents API action variables declared as int[] with a
// non-copyable FixedVector. The user-action response machinery copies all
// arguments into a tuple, so an int[] action fails while compiling main.cpp.
// Carry RAW timings as one copyable string and parse the familiar YAML/CSV
// representation at the component boundary instead.
inline bool parse_signed_timings_text(const std::string &text, std::vector<int32_t> &timings,
                                      std::string &error) {
  timings.clear();
  if (text.empty()) {
    error = "Signal has no timings";
    return false;
  }
  if (text.size() > MAX_SIGNAL_TIMINGS * 16U) {
    error = "Timing text is too long";
    return false;
  }

  std::string normalized = text;
  for (char &character : normalized) {
    if (character == ',' || character == ';' || character == '[' || character == ']')
      character = ' ';
  }

  std::istringstream stream(normalized);
  std::string token;
  while (stream >> token) {
    int32_t value = 0;
    if (!parse_i32(token, value)) {
      error = "Invalid timing value: " + token;
      timings.clear();
      return false;
    }
    if (timings.size() >= MAX_SIGNAL_TIMINGS) {
      error = "Signal has too many timings (maximum 2048)";
      timings.clear();
      return false;
    }
    timings.push_back(value);
  }
  if (timings.empty()) {
    error = "Signal has no timings";
    return false;
  }
  return true;
}

inline bool parse_float_value(const std::string &text, float &value) {
  const std::string trimmed = trim_copy(text);
  if (trimmed.empty()) return false;
  errno = 0;
  char *end = nullptr;
  const float parsed = std::strtof(trimmed.c_str(), &end);
  if (errno != 0 || end == trimmed.c_str() || *end != '\0' || !std::isfinite(parsed)) return false;
  value = parsed;
  return true;
}

inline std::vector<std::string> split_lines(const std::string &text) {
  std::vector<std::string> lines;
  std::istringstream stream(text);
  std::string line;
  while (std::getline(stream, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    lines.push_back(line);
  }
  return lines;
}

inline bool split_key_value(const std::string &line, std::string &key, std::string &value) {
  const auto colon = line.find(':');
  if (colon == std::string::npos) return false;
  key = lower_copy(trim_copy(line.substr(0, colon)));
  value = trim_copy(line.substr(colon + 1));
  return !key.empty();
}

inline bool parse_hex_bytes(const std::string &text, std::vector<uint8_t> &bytes) {
  bytes.clear();
  std::istringstream stream(text);
  std::string token;
  while (stream >> token) {
    if (token.size() != 2) return false;
    uint64_t parsed = 0;
    if (!parse_u64(token, parsed, 16) || parsed > 0xFF) return false;
    bytes.push_back(static_cast<uint8_t>(parsed));
  }
  return !bytes.empty();
}

inline uint32_t little_endian_u32(const std::vector<uint8_t> &bytes) {
  uint32_t value = 0;
  const size_t count = std::min<size_t>(bytes.size(), 4);
  for (size_t i = 0; i < count; i++) value |= static_cast<uint32_t>(bytes[i]) << (8 * i);
  return value;
}

inline uint64_t big_endian_u64(const std::vector<uint8_t> &bytes) {
  uint64_t value = 0;
  for (uint8_t byte : bytes) value = (value << 8U) | byte;
  return value;
}

inline bool normalize_signed_timings(std::vector<int32_t> &timings, std::string &error) {
  std::vector<int32_t> normalized;
  normalized.reserve(timings.size());
  for (int32_t value : timings) {
    if (value == 0) continue;
    // A leading low period is receiver idle, not part of the replayable frame.
    if (normalized.empty() && value < 0) continue;
    if (!normalized.empty() && ((normalized.back() > 0) == (value > 0))) {
      const int64_t sum = static_cast<int64_t>(normalized.back()) + value;
      if (sum > 2000000 || sum < -2000000) {
        error = "Combined duration exceeds 2 seconds";
        return false;
      }
      normalized.back() = static_cast<int32_t>(sum);
    } else {
      normalized.push_back(value);
    }
  }
  timings.swap(normalized);
  return true;
}

inline bool validate_signed_timings(const std::vector<int32_t> &timings, std::string &error,
                                    bool allow_leading_space = false);

// The inexpensive 433 MHz receiver has no RSSI/squelch output and produces
// random transitions when no transmitter is present. Do not mistake such a
// capture for Exact RAW. A real Princeton-like OOK burst has many consecutive
// MARK+SPACE bit periods of nearly the same total length plus at least one
// longer inter-frame SPACE. This function only qualifies the capture; it never
// changes the vector that Exact RAW stores.
inline bool looks_like_regular_ook_burst(const std::vector<int32_t> &input) {
  std::vector<int32_t> normalized = input;
  std::string error;
  if (!normalize_signed_timings(normalized, error) || normalized.size() < 34) return false;

  std::vector<int32_t> periods;
  periods.reserve(normalized.size() / 2);
  size_t long_spaces = 0;
  for (size_t i = 0; i + 1 < normalized.size(); i += 2) {
    if (normalized[i] <= 0 || normalized[i + 1] >= 0) return false;
    const int32_t mark = std::abs(normalized[i]);
    const int32_t space = std::abs(normalized[i + 1]);
    if (space >= 4000) {
      long_spaces++;
      continue;
    }
    const int32_t period = mark + space;
    if (period >= 500 && period <= 10000) periods.push_back(period);
  }
  if (long_spaces == 0 || periods.size() < 16) return false;

  std::sort(periods.begin(), periods.end());
  const int32_t median = periods[periods.size() / 2];
  const int32_t allowed = std::max<int32_t>(250, static_cast<int32_t>(median * 0.35f));
  size_t regular = 0;
  for (const int32_t period : periods)
    if (std::abs(period - median) <= allowed) regular++;
  return regular >= 16 && regular * 100 >= periods.size() * 65;
}

// Cheap 433 MHz receivers often insert a very short opposite-level spike in
// the middle of a valid pulse. That turns one duration into three and prevents
// both rc_switch and frame comparison from seeing the protocol. Estimate the
// shortest real symbol from the lower quartile, remove only much shorter inner
// spikes, and join the two same-level pieces around each spike.
inline void deglitch_rf_timings(std::vector<int32_t> &timings) {
  std::string error;
  if (!normalize_signed_timings(timings, error) || timings.size() < 16) return;

  std::vector<int32_t> symbol_candidates;
  symbol_candidates.reserve(timings.size());
  for (const int32_t value : timings) {
    const int32_t magnitude = std::abs(value);
    if (magnitude >= 40 && magnitude <= 4000) symbol_candidates.push_back(magnitude);
  }
  if (symbol_candidates.size() < 8) return;
  std::sort(symbol_candidates.begin(), symbol_candidates.end());
  const int32_t base = symbol_candidates[symbol_candidates.size() / 4];
  const int32_t spike_limit = std::clamp<int32_t>(static_cast<int32_t>(base * 0.45f), 40, 250);

  size_t i = 1;
  while (i + 1 < timings.size()) {
    if (std::abs(timings[i]) < spike_limit && ((timings[i - 1] > 0) == (timings[i + 1] > 0))) {
      const int64_t joined = static_cast<int64_t>(timings[i - 1]) + timings[i + 1];
      if (joined <= 2000000 && joined >= -2000000) {
        timings[i - 1] = static_cast<int32_t>(joined);
        timings.erase(timings.begin() + i, timings.begin() + i + 2);
        if (i > 1) i--;
        continue;
      }
    }
    i++;
  }
  normalize_signed_timings(timings, error);
}

// RF transmitters normally send the same frame several times. ESP32 RMT closes a
// capture only after the configured idle threshold, so a long idle setting lets
// us observe the real inter-frame guard inside the burst. This helper extracts
// the first complete repeated frame when at least one real guard and the final
// receiver-idle marker are both visible. If repetition cannot be proven, the
// normalized complete capture is returned unchanged.
inline std::vector<int32_t> extract_first_repeated_rf_frame(const std::vector<int32_t> &input,
                                                            bool &frame_extracted) {
  frame_extracted = false;
  std::vector<int32_t> normalized = input;
  std::string normalize_error;
  if (!normalize_signed_timings(normalized, normalize_error) || normalized.size() < 16) return normalized;
  deglitch_rf_timings(normalized);

  std::vector<int32_t> spaces;
  spaces.reserve(normalized.size() / 2);
  for (const int32_t value : normalized) {
    if (value < 0) {
      const int64_t magnitude = -static_cast<int64_t>(value);
      if (magnitude <= std::numeric_limits<int32_t>::max())
        spaces.push_back(static_cast<int32_t>(magnitude));
    }
  }
  if (spaces.size() < 8) return normalized;
  std::sort(spaces.begin(), spaces.end());
  const int32_t typical_space = spaces[(spaces.size() - 1) / 2];
  const int64_t boundary_threshold = std::max<int64_t>(4000, static_cast<int64_t>(typical_space) * 5);

  std::vector<size_t> boundaries;
  // The very first pair can be Princeton's SYNC mark/space. Keep that boundary:
  // it is what aligns a SYNC+DATA capture to the following DATA+SYNC frame.
  for (size_t i = 1; i < normalized.size(); i += 2) {
    if (normalized[i] < 0 && -static_cast<int64_t>(normalized[i]) >= boundary_threshold)
      boundaries.push_back(i);
  }
  if (boundaries.size() < 2) return normalized;

  // Split the burst at every plausible guard. The first segment may start in
  // the middle of a frame, so never assume that the prefix is replayable.
  std::vector<std::vector<int32_t>> segments;
  size_t start = 0;
  for (const size_t boundary : boundaries) {
    if (boundary >= start) {
      std::vector<int32_t> segment(normalized.begin() + start, normalized.begin() + boundary + 1);
      if (segment.size() >= 16 && segment.size() <= MAX_SIGNAL_TIMINGS) segments.push_back(std::move(segment));
    }
    start = boundary + 1;
  }
  if (segments.size() < 2) return normalized;

  float best_ratio = 0.0f;
  size_t best_a = 0;
  size_t best_b = 0;
  for (size_t a = 0; a < segments.size(); a++) {
    for (size_t b = a + 1; b < segments.size(); b++) {
      const int size_delta = std::abs(static_cast<int>(segments[a].size()) - static_cast<int>(segments[b].size()));
      if (size_delta > 2) continue;
      const size_t compared = std::min(segments[a].size(), segments[b].size());
      int good = 0;
      for (size_t i = 0; i < compared; i++) {
        const int32_t av = segments[a][i];
        const int32_t bv = segments[b][i];
        if ((av > 0) != (bv > 0)) continue;
        const int aa = std::abs(av);
        const int bb = std::abs(bv);
        const int allowed = std::max(120, static_cast<int>(std::max(aa, bb) * 0.30f));
        if (std::abs(aa - bb) <= allowed) good++;
      }
      const float ratio = static_cast<float>(good) / static_cast<float>(compared);
      if (ratio > best_ratio) {
        best_ratio = ratio;
        best_a = a;
        best_b = b;
      }
    }
  }
  if (best_ratio < 0.88f) return normalized;

  std::vector<int32_t> frame;
  frame.reserve(segments[best_a].size());
  for (size_t i = 0; i < segments[best_a].size(); i++) {
    if (i + 1 == segments[best_a].size() && segments[best_a][i] < 0 && segments[best_b][i] < 0 &&
        std::max(std::abs(segments[best_a][i]), std::abs(segments[best_b][i])) >= 25000 &&
        std::min(std::abs(segments[best_a][i]), std::abs(segments[best_b][i])) < 25000) {
      frame.push_back(std::abs(segments[best_a][i]) < std::abs(segments[best_b][i])
                          ? segments[best_a][i]
                          : segments[best_b][i]);
      continue;
    }
    const int64_t average = (static_cast<int64_t>(segments[best_a][i]) + segments[best_b][i]) / 2;
    frame.push_back(static_cast<int32_t>(average));
  }
  std::string validation_error;
  if (!validate_signed_timings(frame, validation_error)) return normalized;
  frame_extracted = true;
  return frame;
}

// Fast RAW must never save an arbitrary beginning and end of a held-button
// burst. Even when only one complete cycle exists, two consecutive guard
// boundaries delimit one replayable DATA+SYNC frame. Prefer a measured guard
// over the synthetic receiver-idle value appended after the button is released.
inline std::vector<int32_t> extract_guard_delimited_rf_frame(const std::vector<int32_t> &input,
                                                             bool &frame_aligned) {
  frame_aligned = false;
  std::vector<int32_t> normalized = input;
  std::string error;
  if (!normalize_signed_timings(normalized, error) || normalized.size() < 16) return normalized;
  deglitch_rf_timings(normalized);

  std::vector<int32_t> spaces;
  spaces.reserve(normalized.size() / 2);
  for (const int32_t value : normalized)
    if (value < 0) spaces.push_back(std::abs(value));
  if (spaces.size() < 8) return normalized;
  std::sort(spaces.begin(), spaces.end());
  const int32_t typical_space = spaces[(spaces.size() - 1) / 2];
  const int64_t boundary_threshold = std::max<int64_t>(3500, static_cast<int64_t>(typical_space) * 4);

  std::vector<size_t> boundaries;
  for (size_t i = 1; i < normalized.size(); i += 2)
    if (normalized[i] < 0 && -static_cast<int64_t>(normalized[i]) >= boundary_threshold)
      boundaries.push_back(i);
  if (boundaries.size() < 2) return normalized;

  std::vector<int32_t> fallback;
  for (size_t i = 1; i < boundaries.size(); i++) {
    const size_t start = boundaries[i - 1] + 1;
    const size_t end = boundaries[i];
    if (end < start) continue;
    std::vector<int32_t> candidate(normalized.begin() + start, normalized.begin() + end + 1);
    if (candidate.size() < 18 || candidate.size() > 512 || (candidate.size() % 2) != 0) continue;
    std::string validation_error;
    if (!validate_signed_timings(candidate, validation_error)) continue;
    if (fallback.empty()) fallback = candidate;
    if (std::abs(candidate.back()) < 25000) {
      frame_aligned = true;
      return candidate;
    }
  }
  if (!fallback.empty()) {
    frame_aligned = true;
    return fallback;
  }
  return normalized;
}

inline bool validate_signed_timings(const std::vector<int32_t> &timings, std::string &error,
                                    bool allow_leading_space) {
  if (timings.empty()) {
    error = "Signal has no timings";
    return false;
  }
  if (timings.size() > MAX_SIGNAL_TIMINGS) {
    error = "Signal has too many timings (maximum 2048)";
    return false;
  }
  const bool starts_with_mark = timings.front() > 0;
  if (!starts_with_mark && !allow_leading_space) {
    error = "Signal must start with a positive mark";
    return false;
  }
  for (size_t i = 0; i < timings.size(); i++) {
    const int32_t value = timings[i];
    if (value == 0) {
      error = "A timing cannot be zero";
      return false;
    }
    const bool should_be_positive = starts_with_mark ? (i % 2) == 0 : (i % 2) != 0;
    if ((value > 0) != should_be_positive) {
      error = "Timing signs do not alternate";
      return false;
    }
    const int64_t magnitude = value < 0 ? -static_cast<int64_t>(value) : value;
    if (magnitude > 2000000) {
      error = "A single timing exceeds 2 seconds";
      return false;
    }
  }
  return true;
}

struct PrincetonFrame {
  bool valid{false};
  uint64_t code{0};
  uint8_t bit_count{0};
  uint32_t pulse_length{0};
  uint32_t guard_multiplier{0};
  std::vector<int32_t> timings;
  size_t capture_offset{0};
  uint8_t matching_frames{0};
};

inline PrincetonFrame decode_princeton_frame(const std::vector<int32_t> &frame) {
  PrincetonFrame decoded;
  if (frame.size() < 18 || frame.size() > 130 || (frame.size() % 2) != 0) return decoded;
  const size_t bit_count = (frame.size() - 2) / 2;
  if (bit_count < 8 || bit_count > 64) return decoded;

  // Cheap OOK receivers shift both edges in the same direction: a nominal
  // 403/1209 pair may be measured as roughly 180/1430. The pair total remains
  // close to 4*TE, while estimating TE from the shorter half (the old method)
  // can be badly wrong. Use the median complete-bit period instead.
  std::vector<int32_t> bit_periods;
  bit_periods.reserve(bit_count);
  for (size_t i = 0; i < bit_count; i++)
    bit_periods.push_back(std::abs(frame[i * 2]) + std::abs(frame[i * 2 + 1]));
  std::sort(bit_periods.begin(), bit_periods.end());
  const int32_t typical_period = bit_periods[bit_periods.size() / 2];
  const int32_t te = static_cast<int32_t>(std::lround(static_cast<double>(typical_period) / 4.0));
  if (te < 50 || te > 5000) return decoded;

  uint64_t code = 0;
  for (size_t i = 0; i < bit_count; i++) {
    const int mark = std::abs(frame[i * 2]);
    const int space = std::abs(frame[i * 2 + 1]);
    const int period = mark + space;
    const int short_value = std::min(mark, space);
    const int long_value = std::max(mark, space);
    if (period < static_cast<int>(typical_period * 0.55f) ||
        period > static_cast<int>(typical_period * 1.45f)) return decoded;
    const float ratio = static_cast<float>(long_value) / static_cast<float>(short_value);
    if (short_value < static_cast<int>(te * 0.15f) || short_value > static_cast<int>(te * 2.0f) ||
        long_value < static_cast<int>(te * 1.6f) || long_value > static_cast<int>(te * 5.0f) ||
        ratio < 1.30f) return decoded;
    code = (code << 1U) | (mark > space ? 1U : 0U);
  }

  const int stop_mark = std::abs(frame[bit_count * 2]);
  if (stop_mark < static_cast<int>(te * 0.15f) || stop_mark > static_cast<int>(te * 2.25f)) return decoded;
  const int terminal_space = std::abs(frame[bit_count * 2 + 1]);
  uint32_t guard = static_cast<uint32_t>(std::lround(
      static_cast<double>(terminal_space) / static_cast<double>(te)));
  // A single RMT capture ends with the receiver idle threshold (30 ms), not
  // necessarily the transmitter's real guard. The bit pattern and TE are still
  // valid; use Princeton's canonical guard instead of rejecting the frame.
  if (terminal_space >= 25000 && guard > 72) guard = 30;
  if (guard < 15 || guard > 72) return decoded;

  decoded.valid = true;
  decoded.code = code;
  decoded.bit_count = static_cast<uint8_t>(bit_count);
  decoded.pulse_length = static_cast<uint32_t>(te);
  decoded.guard_multiplier = guard;
  decoded.timings.reserve(bit_count * 2 + 2);
  for (size_t i = bit_count; i > 0; i--) {
    const bool one = (code & (uint64_t{1} << (i - 1))) != 0;
    decoded.timings.push_back(static_cast<int32_t>(te * (one ? 3 : 1)));
    decoded.timings.push_back(-static_cast<int32_t>(te * (one ? 1 : 3)));
  }
  decoded.timings.push_back(te);
  decoded.timings.push_back(-static_cast<int32_t>(te * guard));
  return decoded;
}

// Decode Princeton from either a complete burst or a single RMT capture. This
// is deliberately independent from repeated-frame verification: a successful
// protocol decode (especially when confirmed by on_rc_switch) is already much
// stronger evidence than comparing two noisy RAW fragments.
inline PrincetonFrame decode_princeton_capture(const std::vector<int32_t> &input,
                                                bool expected_code_valid = false,
                                                uint64_t expected_code = 0) {
  std::vector<int32_t> normalized = input;
  std::string error;
  if (!normalize_signed_timings(normalized, error) || normalized.size() < 18) return {};
  deglitch_rf_timings(normalized);

  std::vector<int32_t> spaces;
  for (const int32_t value : normalized)
    if (value < 0) spaces.push_back(std::abs(value));
  if (spaces.empty()) return {};
  std::sort(spaces.begin(), spaces.end());
  const int32_t typical_space = spaces[(spaces.size() - 1) / 2];
  const int64_t boundary_threshold = std::max<int64_t>(3500, static_cast<int64_t>(typical_space) * 4);

  struct CandidateVote {
    PrincetonFrame frame;
    uint8_t count{0};
  };
  std::map<std::pair<uint8_t, uint64_t>, CandidateVote> votes;

  std::vector<size_t> boundaries;
  for (size_t end = 1; end < normalized.size(); end += 2)
    if (normalized[end] < 0 && -static_cast<int64_t>(normalized[end]) >= boundary_threshold)
      boundaries.push_back(end);

  auto add_vote = [&](size_t start, size_t end) {
    if (end < start || normalized[start] <= 0) return false;
    std::vector<int32_t> candidate(normalized.begin() + start, normalized.begin() + end + 1);
    PrincetonFrame decoded = decode_princeton_frame(candidate);
    if (!decoded.valid || (expected_code_valid && decoded.code != expected_code)) return false;
    decoded.capture_offset = start;
    auto &vote = votes[{decoded.bit_count, decoded.code}];
    if (vote.count == 0) vote.frame = decoded;
    if (vote.count < 255) vote.count++;
    return true;
  };

  // First use exact guard-to-guard cycles. This reliably handles captures with
  // a partial prefix followed by a complete Princeton frame.
  for (size_t i = 0; i < boundaries.size(); i++) {
    const size_t end = boundaries[i];
    const size_t start = i == 0 ? 0 : boundaries[i - 1] + 1;
    const size_t count = end - start + 1;
    if (count >= 18 && count <= 130 && (count % 2) == 0 && add_vote(start, end)) continue;

    // If the prefix is partial, search backwards from this guard. Prefer the
    // common 24-bit Princeton length before trying less common lengths.
    if (end + 1 >= 50 && add_vote(end + 1 - 50, end)) continue;
    for (int bits = 64; bits >= 8; bits--) {
      if (bits == 24) continue;
      const size_t count = static_cast<size_t>(bits) * 2U + 2U;
      if (count > end + 1) continue;
      const size_t start = end + 1 - count;
      if (add_vote(start, end)) break;
    }
  }

  CandidateVote *best = nullptr;
  for (auto &entry : votes) {
    CandidateVote &vote = entry.second;
    if (best == nullptr || vote.count > best->count ||
        (vote.count == best->count && vote.frame.bit_count > best->frame.bit_count))
      best = &vote;
  }
  if (best != nullptr) {
    best->frame.matching_frames = best->count;
    return best->frame;
  }

  // A short tap can contain one already aligned DATA+SYNC frame.
  if (normalized.size() <= 130) {
    PrincetonFrame decoded = decode_princeton_frame(normalized);
    if (decoded.valid && (!expected_code_valid || decoded.code == expected_code)) {
      decoded.matching_frames = 1;
      return decoded;
    }
  }
  return {};
}

inline void append_nec_bit(std::vector<int32_t> &timings, bool one) {
  timings.push_back(560);
  timings.push_back(one ? -1690 : -560);
}

inline std::vector<int32_t> build_nec_timings(uint16_t address, uint16_t command) {
  std::vector<int32_t> timings;
  timings.reserve(67);
  timings.push_back(9000);
  timings.push_back(-4500);
  for (uint16_t mask = 1; mask != 0; mask <<= 1U) append_nec_bit(timings, (address & mask) != 0);
  for (uint16_t mask = 1; mask != 0; mask <<= 1U) append_nec_bit(timings, (command & mask) != 0);
  timings.push_back(560);
  return timings;
}

inline std::vector<int32_t> build_princeton_timings(uint64_t code, uint8_t bit_count,
                                                    uint32_t te, uint32_t guard_multiplier) {
  std::vector<int32_t> timings;
  timings.reserve(static_cast<size_t>(bit_count) * 2 + 2);
  for (uint8_t i = bit_count; i > 0; --i) {
    const bool one = (code & (uint64_t{1} << (i - 1))) != 0;
    if (one) {
      timings.push_back(static_cast<int32_t>(te * 3));
      timings.push_back(-static_cast<int32_t>(te));
    } else {
      timings.push_back(static_cast<int32_t>(te));
      timings.push_back(-static_cast<int32_t>(te * 3));
    }
  }
  timings.push_back(static_cast<int32_t>(te));
  timings.push_back(-static_cast<int32_t>(te * guard_multiplier));
  return timings;
}

inline std::vector<int32_t> build_dooya_timings(uint64_t code, uint8_t bit_count = 40,
                                                uint32_t te_short = 366,
                                                uint32_t te_long = 733) {
  // Mirrors Flipper's official Dooya encoder. Its upload length is
  // bit_count*2+2: the final data SPACE is deliberately omitted because the
  // leading SPACE of the next repetition closes that bit and frame.
  std::vector<int32_t> timings;
  timings.reserve(static_cast<size_t>(bit_count) * 2U + 2U);
  const uint32_t header_space = (code & 1U) != 0 ? te_long * 13U
                                                 : te_long * 12U + te_short;
  timings.push_back(-static_cast<int32_t>(header_space));
  timings.push_back(static_cast<int32_t>(te_short * 13U));
  timings.push_back(-static_cast<int32_t>(te_long * 2U));
  for (uint8_t i = bit_count; i > 0; --i) {
    const bool one = (code & (uint64_t{1} << (i - 1U))) != 0;
    timings.push_back(static_cast<int32_t>(one ? te_long : te_short));
    if (i > 1U) timings.push_back(-static_cast<int32_t>(one ? te_short : te_long));
  }
  return timings;
}

inline std::vector<int32_t> encode_record(const ParsedSignal &signal) {
  std::vector<int32_t> record;
  record.reserve(SIGNAL_HEADER_WORDS + signal.timings.size());
  record.push_back(SIGNAL_MAGIC_1);
  record.push_back(SIGNAL_MAGIC_2);
  record.push_back(SIGNAL_VERSION);
  record.push_back(static_cast<int32_t>(signal.kind));
  record.push_back(static_cast<int32_t>(signal.frequency));
  record.push_back(static_cast<int32_t>(signal.duty_percent));
  record.push_back(static_cast<int32_t>(signal.recommended_repeats));
  record.push_back(static_cast<int32_t>(signal.timings.size()));
  record.push_back(static_cast<int32_t>(signal.encoding));
  record.push_back(static_cast<int32_t>(signal.protocol));
  record.push_back(static_cast<int32_t>(signal.bit_count));
  record.push_back(static_cast<int32_t>(signal.pulse_length));
  record.push_back(static_cast<int32_t>(signal.guard_multiplier));
  record.push_back(static_cast<int32_t>(static_cast<uint32_t>(signal.code >> 32U)));
  record.push_back(static_cast<int32_t>(static_cast<uint32_t>(signal.code)));
  record.insert(record.end(), signal.timings.begin(), signal.timings.end());
  return record;
}

inline bool decode_record(const std::vector<int32_t> &record, StoredSignal &signal, std::string *error = nullptr) {
  auto fail = [&](const std::string &message) {
    if (error != nullptr) *error = message;
    return false;
  };
  if (record.size() < SIGNAL_V2_HEADER_WORDS) return fail("Record header is missing");
  if (record[0] != SIGNAL_MAGIC_1 || record[1] != SIGNAL_MAGIC_2) return fail("Unknown record format");
  if (record[2] != 2 && record[2] != SIGNAL_VERSION) return fail("Unsupported record version");
  if (record[3] != static_cast<int32_t>(SignalKind::IR) &&
      record[3] != static_cast<int32_t>(SignalKind::RF))
    return fail("Invalid record type");
  if (record[7] <= 0 || static_cast<size_t>(record[7]) > MAX_SIGNAL_TIMINGS)
    return fail("Invalid timing count");
  const size_t header_words = record[2] == 2 ? SIGNAL_V2_HEADER_WORDS : SIGNAL_HEADER_WORDS;
  if (record.size() != header_words + static_cast<size_t>(record[7]))
    return fail("Record length does not match its header");

  signal.kind = static_cast<SignalKind>(record[3]);
  signal.frequency = static_cast<uint32_t>(record[4]);
  signal.duty_percent = static_cast<uint8_t>(std::clamp<int32_t>(record[5], int32_t{1}, int32_t{100}));
  signal.recommended_repeats = static_cast<uint8_t>(std::clamp<int32_t>(record[6], int32_t{1}, int32_t{20}));
  if (record[2] == SIGNAL_VERSION) {
    if (record[8] < static_cast<int32_t>(SignalEncoding::RAW) ||
        record[8] > static_cast<int32_t>(SignalEncoding::DOOYA))
      return fail("Invalid record encoding");
    signal.encoding = static_cast<SignalEncoding>(record[8]);
    signal.protocol = static_cast<uint8_t>(std::clamp<int32_t>(record[9], 0, 255));
    signal.bit_count = static_cast<uint8_t>(std::clamp<int32_t>(record[10], 0, 64));
    signal.pulse_length = static_cast<uint32_t>(std::max<int32_t>(record[11], 0));
    signal.guard_multiplier = static_cast<uint32_t>(std::max<int32_t>(record[12], 0));
    signal.code = (static_cast<uint64_t>(static_cast<uint32_t>(record[13])) << 32U) |
                  static_cast<uint32_t>(record[14]);
  } else {
    signal.encoding = SignalEncoding::RAW;
  }
  signal.timings.assign(record.begin() + header_words, record.end());
  std::string timing_error;
  const bool allow_leading_space =
      (signal.encoding == SignalEncoding::RC_SWITCH && signal.protocol == 6) ||
      signal.encoding == SignalEncoding::DOOYA;
  if (!validate_signed_timings(signal.timings, timing_error, allow_leading_space))
    return fail(timing_error);
  return true;
}

inline bool parse_ir_file(const std::string &text, ParsedSignal &signal, std::string &error) {
  const auto lines = split_lines(text);
  std::string filetype;
  std::vector<std::map<std::string, std::string>> blocks;
  std::map<std::string, std::string> current;

  auto flush_block = [&]() {
    if (current.find("type") != current.end()) {
      blocks.push_back(current);
      current.clear();
    }
  };

  for (const auto &raw_line : lines) {
    const std::string line = trim_copy(raw_line);
    if (line.empty()) continue;
    if (line[0] == '#') {
      flush_block();
      continue;
    }
    std::string key;
    std::string value;
    if (!split_key_value(line, key, value)) continue;
    if (key == "filetype") {
      filetype = value;
      continue;
    }
    if (key == "version") continue;
    current[key] = value;
  }
  flush_block();

  if (lower_copy(filetype) != "ir signals file") {
    error = "This is not a Flipper IR signals file";
    return false;
  }
  if (blocks.empty()) {
    error = "IR file contains no signal";
    return false;
  }

  const std::map<std::string, std::string> *selected = nullptr;
  for (const auto &block : blocks) {
    const auto type_it = block.find("type");
    if (type_it == block.end()) continue;
    const std::string type = lower_copy(type_it->second);
    if (type == "raw") {
      selected = &block;
      break;
    }
    if (type == "parsed") {
      const auto proto_it = block.find("protocol");
      if (proto_it != block.end()) {
        const std::string protocol = lower_copy(proto_it->second);
        if (protocol == "nec" || protocol == "necext") {
          selected = &block;
          break;
        }
      }
    }
  }
  if (selected == nullptr) {
    error = "Supported IR formats are raw and parsed NEC/NECext";
    return false;
  }

  const auto &block = *selected;
  const std::string type = lower_copy(block.at("type"));
  const auto name_it = block.find("name");
  signal.name = name_it == block.end() ? "IR signal" : name_it->second;
  signal.kind = SignalKind::IR;
  signal.recommended_repeats = 1;

  if (type == "raw") {
    const auto frequency_it = block.find("frequency");
    const auto duty_it = block.find("duty_cycle");
    const auto data_it = block.find("data");
    if (frequency_it == block.end() || duty_it == block.end() || data_it == block.end()) {
      error = "Raw IR requires frequency, duty_cycle and data";
      return false;
    }
    uint64_t frequency = 0;
    if (!parse_u64(frequency_it->second, frequency) || frequency < 20000 || frequency > 60000) {
      error = "Raw IR frequency must be in range 20000..60000 Hz";
      return false;
    }
    float duty = 0.0f;
    if (!parse_float_value(duty_it->second, duty) || duty < 0.05f || duty > 1.0f) {
      error = "Invalid IR duty cycle";
      return false;
    }
    std::istringstream data_stream(data_it->second);
    uint64_t duration = 0;
    size_t index = 0;
    signal.timings.clear();
    while (data_stream >> duration) {
      if (duration == 0 || duration > 2000000 || signal.timings.size() >= MAX_SIGNAL_TIMINGS) {
        error = "Invalid raw IR timings";
        return false;
      }
      signal.timings.push_back((index % 2 == 0) ? static_cast<int32_t>(duration)
                                                : -static_cast<int32_t>(duration));
      index++;
    }
    if (signal.timings.empty()) {
      error = "Raw IR signal has no data";
      return false;
    }
    signal.frequency = static_cast<uint32_t>(frequency);
    signal.duty_percent = static_cast<uint8_t>(std::clamp<int>(static_cast<int>(std::lround(duty * 100.0f)), 1, 100));
    signal.encoding = SignalEncoding::RAW;
    signal.source_format = "Flipper IR raw";
  } else {
    const auto protocol_it = block.find("protocol");
    const auto address_it = block.find("address");
    const auto command_it = block.find("command");
    if (protocol_it == block.end() || address_it == block.end() || command_it == block.end()) {
      error = "Parsed NEC/NECext requires protocol, address and command";
      return false;
    }
    const std::string protocol = lower_copy(protocol_it->second);
    if (protocol != "nec" && protocol != "necext") {
      error = "The supported parsed IR protocols are NEC and NECext";
      return false;
    }
    std::vector<uint8_t> address_bytes;
    std::vector<uint8_t> command_bytes;
    if (!parse_hex_bytes(address_it->second, address_bytes) || address_bytes.size() != 4 ||
        !parse_hex_bytes(command_it->second, command_bytes) || command_bytes.size() != 4) {
      error = "NEC/NECext address and command must contain four bytes each";
      return false;
    }
    const uint32_t address32 = little_endian_u32(address_bytes);
    const uint32_t command32 = little_endian_u32(command_bytes);
    uint16_t address = 0;
    uint16_t command = 0;
    if (protocol == "nec") {
      if (address32 > 0xFF || command32 > 0xFF) {
        error = "Flipper NEC requires 8-bit address and command values";
        return false;
      }
      // Flipper stores the logical 8-bit values. A standard NEC frame transmits
      // each value followed by its one's complement.
      address = static_cast<uint16_t>(address32 | ((~address32 & 0xFFU) << 8U));
      command = static_cast<uint16_t>(command32 | ((~command32 & 0xFFU) << 8U));
    } else {
      if (address32 > 0xFFFF || command32 > 0xFFFF) {
        error = "Flipper NECext requires 16-bit address and command values";
        return false;
      }
      // Flipper stores both 16-bit fields exactly as transmitted for NECext.
      // Do not synthesize complements: doing so would change valid extended
      // addresses and commands such as 00 EF / 02 FD.
      address = static_cast<uint16_t>(address32);
      command = static_cast<uint16_t>(command32);
    }
    signal.timings = build_nec_timings(address, command);
    signal.frequency = 38000;
    signal.duty_percent = 33;
    signal.encoding = SignalEncoding::NEC;
    signal.bit_count = 32;
    signal.pulse_length = 560;
    signal.code = (static_cast<uint64_t>(address) << 16U) | command;
    signal.source_format = protocol == "nec" ? "Flipper IR parsed NEC" : "Flipper IR parsed NECext";
  }

  return validate_signed_timings(signal.timings, error);
}

inline bool parse_rf_raw_lines(const std::vector<std::pair<std::string, std::string>> &fields,
                               std::vector<int32_t> &timings, std::string &error) {
  timings.clear();
  for (const auto &field : fields) {
    if (field.first != "raw_data") continue;
    std::istringstream stream(field.second);
    std::string token;
    while (stream >> token) {
      int32_t value = 0;
      if (!parse_i32(token, value) || value == 0) {
        error = "Invalid RAW_Data value";
        return false;
      }
      if (timings.size() >= MAX_SIGNAL_TIMINGS) {
        error = "Too many RAW_Data values";
        return false;
      }
      timings.push_back(value);
    }
  }
  if (timings.empty()) {
    error = "RAW_Data is missing";
    return false;
  }
  if (!normalize_signed_timings(timings, error)) return false;
  return validate_signed_timings(timings, error);
}

inline bool parse_sub_file(const std::string &text, ParsedSignal &signal, std::string &error) {
  const auto lines = split_lines(text);
  std::vector<std::pair<std::string, std::string>> fields;
  std::map<std::string, std::string> last_value;
  for (const auto &raw_line : lines) {
    const std::string line = trim_copy(raw_line);
    if (line.empty() || line[0] == '#') continue;
    std::string key;
    std::string value;
    if (!split_key_value(line, key, value)) continue;
    fields.emplace_back(key, value);
    last_value[key] = value;
  }

  auto get = [&](const char *key) -> const std::string * {
    const auto it = last_value.find(key);
    return it == last_value.end() ? nullptr : &it->second;
  };

  const std::string *filetype = get("filetype");
  const std::string *frequency_text = get("frequency");
  const std::string *preset = get("preset");
  const std::string *protocol_text = get("protocol");
  if (filetype == nullptr || lower_copy(*filetype).find("flipper subghz") == std::string::npos) {
    error = "This is not a Flipper SubGhz file";
    return false;
  }
  if (frequency_text == nullptr || preset == nullptr || protocol_text == nullptr) {
    error = ".sub file is missing Frequency, Preset or Protocol";
    return false;
  }

  uint64_t frequency = 0;
  if (!parse_u64(*frequency_text, frequency) || frequency > std::numeric_limits<int32_t>::max()) {
    error = "Invalid RF frequency";
    return false;
  }
  const int64_t delta = static_cast<int64_t>(frequency) - 433920000LL;
  if (std::llabs(delta) > 300000LL) {
    error = "AR01V3 supports OOK/ASK near 433.92 MHz only";
    return false;
  }
  if (lower_copy(*preset).find("ook") == std::string::npos) {
    error = "RF preset is not OOK/ASK";
    return false;
  }

  signal.kind = SignalKind::RF;
  signal.name = "RF signal";
  signal.frequency = static_cast<uint32_t>(frequency);
  signal.duty_percent = 100;
  const std::string protocol = lower_copy(*protocol_text);

  if (protocol == "raw") {
    if (!parse_rf_raw_lines(fields, signal.timings, error)) return false;
    signal.recommended_repeats = 1;
    signal.encoding = SignalEncoding::RAW;
    signal.source_format = "Flipper SubGhz RAW";
    return true;
  }

  if (protocol == "dooya") {
    const std::string *bit_text = get("bit");
    const std::string *key_text = get("key");
    if (bit_text == nullptr || key_text == nullptr) {
      error = "Dooya requires Bit and Key";
      return false;
    }
    uint64_t bit_count = 0;
    if (!parse_u64(*bit_text, bit_count) || bit_count != 40) {
      error = "Dooya requires exactly 40 bits";
      return false;
    }
    std::vector<uint8_t> key_bytes;
    if (!parse_hex_bytes(*key_text, key_bytes) || key_bytes.size() != 8) {
      error = "Dooya Key must contain eight bytes";
      return false;
    }
    const uint64_t key = big_endian_u64(key_bytes);
    if ((key >> 40U) != 0) {
      error = "Dooya 40-bit Key must use the last five bytes";
      return false;
    }
    uint64_t repeats = 5;
    if (const std::string *repeat_text = get("repeat"); repeat_text != nullptr) {
      if (!parse_u64(*repeat_text, repeats) || repeats < 1 || repeats > 20) {
        error = "Repeat must be in range 1..20";
        return false;
      }
    }
    signal.timings = build_dooya_timings(key);
    signal.recommended_repeats = static_cast<uint8_t>(repeats);
    signal.encoding = SignalEncoding::DOOYA;
    signal.bit_count = 40;
    signal.pulse_length = 366;
    signal.code = key;
    signal.source_format = "Flipper SubGhz Dooya";
    return validate_signed_timings(signal.timings, error, true);
  }

  if (protocol != "princeton") {
    error = "Supported .sub protocols are Princeton, Dooya and RAW";
    return false;
  }

  const std::string *bit_text = get("bit");
  const std::string *key_text = get("key");
  const std::string *te_text = get("te");
  if (bit_text == nullptr || key_text == nullptr || te_text == nullptr) {
    error = "Princeton requires Bit, Key and TE";
    return false;
  }
  uint64_t bit_count = 0;
  uint64_t te = 0;
  if (!parse_u64(*bit_text, bit_count) || bit_count < 1 || bit_count > 64) {
    error = "Princeton bit count must be in range 1..64";
    return false;
  }
  if (!parse_u64(*te_text, te) || te < 50 || te > 5000) {
    error = "Princeton TE must be in range 50..5000 us";
    return false;
  }
  std::vector<uint8_t> key_bytes;
  if (!parse_hex_bytes(*key_text, key_bytes) || key_bytes.size() != 8) {
    error = "Princeton Key must contain eight bytes";
    return false;
  }
  const uint64_t key = big_endian_u64(key_bytes);

  uint64_t guard_time = 30;
  if (const std::string *guard_text = get("guard_time"); guard_text != nullptr) {
    if (!parse_u64(*guard_text, guard_time)) {
      error = "Invalid Guard_time";
      return false;
    }
    if (guard_time < 15 || guard_time > 72) guard_time = 30;
  }
  uint64_t repeats = 5;
  if (const std::string *repeat_text = get("repeat"); repeat_text != nullptr) {
    if (!parse_u64(*repeat_text, repeats) || repeats < 1 || repeats > 20) {
      error = "Repeat must be in range 1..20";
      return false;
    }
  }

  signal.timings = build_princeton_timings(key, static_cast<uint8_t>(bit_count),
                                           static_cast<uint32_t>(te),
                                           static_cast<uint32_t>(guard_time));
  signal.recommended_repeats = static_cast<uint8_t>(repeats);
  signal.encoding = SignalEncoding::PRINCETON;
  signal.protocol = 1;
  signal.bit_count = static_cast<uint8_t>(bit_count);
  signal.pulse_length = static_cast<uint32_t>(te);
  signal.guard_multiplier = static_cast<uint32_t>(guard_time);
  signal.code = key;
  signal.source_format = "Flipper SubGhz Princeton";
  return validate_signed_timings(signal.timings, error);
}

inline bool parse_flipper_file(const std::string &text, ParsedSignal &signal, std::string &error) {
  if (text.size() > MAX_FLIPPER_FILE_BYTES) {
    error = "File exceeds 12 KiB";
    return false;
  }
  const std::string lower = lower_copy(text.substr(0, std::min<size_t>(text.size(), 256)));
  if (lower.find("filetype: ir signals file") != std::string::npos) return parse_ir_file(text, signal, error);
  if (lower.find("filetype: flipper subghz") != std::string::npos) return parse_sub_file(text, signal, error);
  error = "File is not a recognized Flipper .ir or .sub file";
  return false;
}

}  // namespace esphome::flipper_importer
