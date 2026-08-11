#include "../esphome/components/flipper_importer/flipper_parser.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

using namespace esphome::flipper_importer;

static std::string read_file(const char *path) {
  std::ifstream file(path, std::ios::binary);
  if (!file) throw std::runtime_error(std::string("Cannot open: ") + path);
  std::ostringstream out;
  out << file.rdbuf();
  return out.str();
}

static void require(bool condition, const std::string &message) {
  if (!condition) throw std::runtime_error(message);
}

int main(int argc, char **argv) {
  try {
    require(argc == 4, "Usage: test_flipper_parser <princeton.sub> <file.ir> <dooya.sub>");

    std::vector<int32_t> api_timings;
    std::string api_error;
    require(parse_signed_timings_text("[9000, -4500, 560, -560]", api_timings, api_error),
            "HA RAW text: failed to parse YAML-style list: " + api_error);
    require(api_timings == std::vector<int32_t>({9000, -4500, 560, -560}),
            "HA RAW text: parser changed the timings");
    api_error.clear();
    require(parse_signed_timings_text("403 -1209; 1209,-403", api_timings, api_error),
            "HA RAW text: failed to parse CSV: " + api_error);
    require(api_timings == std::vector<int32_t>({403, -1209, 1209, -403}),
            "HA RAW text: CSV parser changed the timings");
    api_error.clear();
    require(!parse_signed_timings_text("403,broken,-403", api_timings, api_error),
            "HA RAW text: invalid token was accepted");

    ParsedSignal rf;
    std::string error;
    require(parse_flipper_file(read_file(argv[1]), rf, error), "RF parse: " + error);
    require(rf.kind == SignalKind::RF, "RF: wrong signal kind");
    require(rf.source_format == "Flipper SubGhz Princeton", "RF: wrong source format");
    require(rf.frequency == 433920000U, "RF: wrong frequency");
    require(rf.recommended_repeats == 5U, "RF: wrong repeat count");
    require(rf.encoding == SignalEncoding::PRINCETON && rf.bit_count == 24 && rf.pulse_length == 403,
            "RF: missing Princeton metadata");
    require(rf.timings.size() == 50U, "RF: expected 50 timings");
    require(rf.timings[0] == 1209 && rf.timings[1] == -403, "RF: invalid first Princeton bit");
    require(rf.timings[46] == 1209 && rf.timings[47] == -403, "RF: invalid last data bit");
    require(rf.timings[48] == 403 && rf.timings[49] == -12090, "RF: invalid stop/guard");

    // Emulate reception of three repeats through RMT. With RF idle=30 ms,
    // the real Guard_time=12090 us remains between frames, while releasing
    // the button produces the synthetic final -30000 us duration.
    std::vector<int32_t> repeated_burst;
    for (int repeat = 0; repeat < 3; repeat++)
      repeated_burst.insert(repeated_burst.end(), rf.timings.begin(), rf.timings.end());
    repeated_burst.back() = -30000;
    bool frame_extracted = false;
    const auto learned_frame = extract_first_repeated_rf_frame(repeated_burst, frame_extracted);
    require(frame_extracted, "RF learning: repeated frame was not detected");
    require(learned_frame == rf.timings,
            "RF learning: first frame and Guard_time were not preserved");

    // Capture may begin in the middle of a held-button burst. The extractor
    // must skip the partial prefix and select two matching complete frames.
    std::vector<int32_t> mid_frame(rf.timings.begin() + 20, rf.timings.end());
    mid_frame.insert(mid_frame.end(), rf.timings.begin(), rf.timings.end());
    mid_frame.insert(mid_frame.end(), rf.timings.begin(), rf.timings.end());
    mid_frame.back() = -30000;
    frame_extracted = false;
    const auto aligned_frame = extract_first_repeated_rf_frame(mid_frame, frame_extracted);
    require(frame_extracted && aligned_frame == rf.timings,
            "RF learning: capture beginning mid-burst produced a partial frame");

    std::vector<int32_t> single_frame = rf.timings;
    single_frame.back() = -30000;
    frame_extracted = false;
    const auto unproven_frame = extract_first_repeated_rf_frame(single_frame, frame_extracted);
    require(!frame_extracted && unproven_frame.back() == -30000,
            "RF learning: a single frame was incorrectly treated as confirmed");
    const auto single_princeton = decode_princeton_capture(single_frame, true, rf.code);
    require(single_princeton.valid && single_princeton.code == rf.code &&
                single_princeton.bit_count == 24 && single_princeton.pulse_length == 403 &&
                single_princeton.guard_multiplier == 30 && single_princeton.timings == rf.timings,
            "RF learning: valid single Princeton frame with -30000 idle was rejected");

    // A capture can start in a partial frame and still contain one complete
    // protocol frame. Protocol decoding must find it without requiring a pair.
    std::vector<int32_t> partial_then_single(rf.timings.begin() + 20, rf.timings.end());
    partial_then_single.insert(partial_then_single.end(), single_frame.begin(), single_frame.end());
    const auto recovered_princeton = decode_princeton_capture(partial_then_single, true, rf.code);
    require(recovered_princeton.valid && recovered_princeton.timings == rf.timings,
            "RF learning: complete Princeton frame was not found after a partial prefix");

    // ESPHome's official RC-Switch transmitter emits SYNC before the data bits.
    // A real RMT capture can therefore contain SYNC+DATA rather than the
    // Flipper DATA+SYNC representation. The first guard must be used as an
    // alignment boundary, and the following complete cyclic frames recovered.
    std::vector<int32_t> sync_first_frame;
    sync_first_frame.push_back(rf.timings[48]);
    sync_first_frame.push_back(rf.timings[49]);
    sync_first_frame.insert(sync_first_frame.end(), rf.timings.begin(), rf.timings.begin() + 48);
    require(sync_first_frame.size() == 50U, "RF SYNC-first: wrong test-frame length");
    std::vector<int32_t> sync_first_burst;
    for (int repeat = 0; repeat < 3; repeat++)
      sync_first_burst.insert(sync_first_burst.end(), sync_first_frame.begin(), sync_first_frame.end());
    sync_first_burst.back() = -30000;
    const auto sync_first_decoded = decode_princeton_capture(sync_first_burst, false, 0);
    require(sync_first_decoded.valid && sync_first_decoded.code == rf.code &&
                sync_first_decoded.bit_count == 24 && sync_first_decoded.matching_frames >= 2,
            "RF SYNC-first: decoder did not find repeated Princeton frames");
    frame_extracted = false;
    const auto sync_first_extracted = extract_first_repeated_rf_frame(sync_first_burst, frame_extracted);
    require(frame_extracted && sync_first_extracted == rf.timings,
            "RF SYNC-first: RAW fallback was not aligned to DATA+SYNC");

    // Reproduce a typical ~130-timing capture: the receiver starts 20
    // timings into a three-frame, 150-timing SYNC-first burst. At least one
    // complete Princeton frame remains and must still decode by protocol.
    std::vector<int32_t> sync_first_130(sync_first_burst.begin() + 20, sync_first_burst.end());
    require(sync_first_130.size() == 130U, "RF SYNC-first: wrong 130-timing test length");
    const auto sync_first_130_decoded = decode_princeton_capture(sync_first_130, false, 0);
    require(sync_first_130_decoded.valid && sync_first_130_decoded.code == rf.code &&
                sync_first_130_decoded.bit_count == 24,
            "RF SYNC-first: partial 130-timing capture was not recognized");

    // Reproduce the reported 137->136 shape and the duty-cycle distortion of
    // the inexpensive OOK receiver. Every bit still occupies 4*TE, but its
    // short half can be far shorter than the old decoder allowed. Starting at
    // index 13 also produces a leading negative duration removed by normalize.
    std::vector<int32_t> distorted_sync_burst = sync_first_burst;
    for (size_t frame_start = 0; frame_start < 150; frame_start += 50) {
      distorted_sync_burst[frame_start] = 260;
      for (size_t pair = frame_start + 2; pair + 1 < frame_start + 50; pair += 2) {
        const int32_t period = std::abs(distorted_sync_burst[pair]) +
                               std::abs(distorted_sync_burst[pair + 1]);
        if (distorted_sync_burst[pair] > std::abs(distorted_sync_burst[pair + 1])) {
          distorted_sync_burst[pair] = period - 90;
          distorted_sync_burst[pair + 1] = -90;
        } else {
          distorted_sync_burst[pair] = 170;
          distorted_sync_burst[pair + 1] = -(period - 170);
        }
      }
    }
    distorted_sync_burst.back() = -30000;
    std::vector<int32_t> reported_137(distorted_sync_burst.begin() + 13,
                                      distorted_sync_burst.end());
    require(reported_137.size() == 137U, "RF robust: wrong 137-timing test length");
    const auto reported_137_decoded = decode_princeton_capture(reported_137, false, 0);
    require(reported_137_decoded.valid && reported_137_decoded.code == rf.code &&
                reported_137_decoded.bit_count == 24 && reported_137_decoded.pulse_length == 403,
            "RF robust: distorted 137->136 capture was not recognized");
    bool guard_aligned = false;
    const auto fast_raw_frame = extract_guard_delimited_rf_frame(reported_137, guard_aligned);
    require(guard_aligned && fast_raw_frame.size() == 50U,
            "RF Fast RAW: complete frame between guards was not selected");

    // Exact RAW must retain the complete captured burst. The only permitted
    // canonicalization is removal of leading idle and lossless joining of
    // adjacent durations with the same level; no frame extraction is applied.
    std::vector<int32_t> exact_raw = reported_137;
    error.clear();
    require(normalize_signed_timings(exact_raw, error), "RF Exact RAW normalize: " + error);
    require(exact_raw.size() == 136U && exact_raw != fast_raw_frame,
            "RF Exact RAW: burst was heuristically reduced to a single frame");
    ParsedSignal captured_signal;
    captured_signal.kind = SignalKind::RF;
    captured_signal.frequency = 433920000;
    captured_signal.duty_percent = 100;
    captured_signal.recommended_repeats = 1;
    captured_signal.encoding = SignalEncoding::CAPTURED_RAW;
    captured_signal.timings = exact_raw;
    const auto captured_record = encode_record(captured_signal);
    StoredSignal captured_decoded;
    error.clear();
    require(decode_record(captured_record, captured_decoded, &error),
            "RF Exact RAW record: " + error);
    require(captured_decoded.encoding == SignalEncoding::CAPTURED_RAW &&
                captured_decoded.recommended_repeats == 1U &&
                captured_decoded.timings == exact_raw,
            "RF Exact RAW: NVS record changed the captured burst");
    require(looks_like_regular_ook_burst(reported_137),
            "RF noise gate: regular Princeton 137->136 burst was rejected");

    std::vector<int32_t> ambient_noise;
    for (int i = 0; i < 110; i++) {
      ambient_noise.push_back(220 + ((i * 137) % 2800));
      ambient_noise.push_back(i % 29 == 0 ? -9000 : -(240 + ((i * 521) % 3300)));
    }
    require(!looks_like_regular_ook_burst(ambient_noise),
            "RF noise gate: random 219/220-timing noise was treated as a remote signal");

    // Simulate receiver glitches that split valid MARK/SPACE durations into
    // three parts. Deglitching must restore the exact frame before decoding.
    std::vector<int32_t> glitched_princeton = single_frame;
    glitched_princeton[4] = 500;
    glitched_princeton.insert(glitched_princeton.begin() + 5, {-55, 709});
    glitched_princeton[13] = -200;
    glitched_princeton.insert(glitched_princeton.begin() + 14, {60, -203});
    require(glitched_princeton.size() == single_frame.size() + 4,
            "RF deglitch test: invalid glitch simulation");
    deglitch_rf_timings(glitched_princeton);
    require(glitched_princeton == single_frame,
            "RF deglitch: short receiver glitches were not merged");
    const auto deglitched_decode = decode_princeton_capture(glitched_princeton, true, rf.code);
    require(deglitched_decode.valid && deglitched_decode.timings == rf.timings,
            "RF deglitch: cleaned Princeton frame was not recognized");

    const auto rf_record = encode_record(rf);
    StoredSignal rf_decoded;
    require(decode_record(rf_record, rf_decoded, &error), "RF record: " + error);
    require(rf_decoded.kind == SignalKind::RF && rf_decoded.timings == rf.timings,
            "RF record: decoded data differs from input");

    // ESPHome RC-Switch protocol 6 is inverted. Its canonical transmitter
    // waveform starts with a SPACE, unlike all imported RAW/Princeton records.
    ParsedSignal protocol6;
    protocol6.kind = SignalKind::RF;
    protocol6.frequency = 433920000;
    protocol6.duty_percent = 100;
    protocol6.recommended_repeats = 5;
    protocol6.encoding = SignalEncoding::RC_SWITCH;
    protocol6.protocol = 6;
    protocol6.bit_count = 24;
    protocol6.code = 0xFFFF01;
    protocol6.timings = {-10350, 450};
    for (int bit = 23; bit >= 0; --bit) {
      const bool one = (protocol6.code & (uint64_t{1} << bit)) != 0;
      protocol6.timings.push_back(one ? -900 : -450);
      protocol6.timings.push_back(one ? 450 : 900);
    }
    error.clear();
    require(!validate_signed_timings(protocol6.timings, error),
            "RF protocol 6: generic RAW incorrectly accepted a leading SPACE");
    error.clear();
    require(validate_signed_timings(protocol6.timings, error, true),
            "RF protocol 6: inverted timings were rejected: " + error);
    const auto protocol6_record = encode_record(protocol6);
    StoredSignal protocol6_decoded;
    error.clear();
    require(decode_record(protocol6_record, protocol6_decoded, &error),
            "RF protocol 6 record: " + error);
    require(protocol6_decoded.protocol == 6 && protocol6_decoded.code == 0xFFFF01 &&
                protocol6_decoded.timings == protocol6.timings,
            "RF protocol 6: record changed the code or timings");

    ParsedSignal dooya;
    error.clear();
    require(parse_flipper_file(read_file(argv[3]), dooya, error), "Dooya parse: " + error);
    require(dooya.kind == SignalKind::RF, "Dooya: wrong signal kind");
    require(dooya.source_format == "Flipper SubGhz Dooya", "Dooya: wrong source format");
    require(dooya.frequency == 433920000U, "Dooya: wrong frequency");
    require(dooya.encoding == SignalEncoding::DOOYA && dooya.bit_count == 40 &&
                dooya.pulse_length == 366 && dooya.code == 0x123456789AULL,
            "Dooya: invalid metadata or key");
    require(dooya.recommended_repeats == 5U, "Dooya: wrong default repeat count");
    require(dooya.timings.size() == 82U, "Dooya: expected 82 timings from the reference upload");
    require(dooya.timings[0] == -9162 && dooya.timings[1] == 4758 &&
                dooya.timings[2] == -1466,
            "Dooya: invalid header");
    require(dooya.timings[3] == 366 && dooya.timings[4] == -733,
            "Dooya: invalid encoding of the first zero bit");
    require(dooya.timings.back() == 366,
            "Dooya: final zero bit does not end with MARK before the next header");
    error.clear();
    require(validate_signed_timings(dooya.timings, error, true),
            "Dooya: waveform beginning with SPACE was rejected: " + error);
    const auto dooya_record = encode_record(dooya);
    StoredSignal dooya_decoded;
    error.clear();
    require(decode_record(dooya_record, dooya_decoded, &error), "Dooya record: " + error);
    require(dooya_decoded.encoding == SignalEncoding::DOOYA &&
                dooya_decoded.code == dooya.code && dooya_decoded.timings == dooya.timings,
            "Dooya: NVS record changed the code or waveform");

    ParsedSignal ir;
    error.clear();
    require(parse_flipper_file(read_file(argv[2]), ir, error), "IR parse: " + error);
    require(ir.kind == SignalKind::IR, "IR: wrong signal kind");
    require(ir.name == "Sample_NEC", "IR: wrong name");
    require(ir.source_format == "Flipper IR parsed NEC", "IR: wrong source format");
    require(ir.encoding == SignalEncoding::NEC && ir.code == 0x817ED52AULL,
            "IR: invalid standard NEC complements");
    require(ir.frequency == 38000U, "IR: wrong frequency");
    require(ir.duty_percent == 33U, "IR: wrong duty cycle");
    require(ir.recommended_repeats == 1U, "IR: wrong repeat count");
    require(ir.timings.size() == 67U, "IR: expected 67 timings");
    require(ir.timings[0] == 9000 && ir.timings[1] == -4500, "IR: invalid NEC header");
    require(ir.timings.back() == 560, "IR: missing final NEC MARK");

    // Address 0x007E, LSB first: 0,1,1,1,1,1,1,0...
    require(ir.timings[2] == 560 && ir.timings[3] == -560, "IR: address bit 0");
    require(ir.timings[4] == 560 && ir.timings[5] == -1690, "IR: address bit 1");
    require(ir.timings[18] == 560 && ir.timings[19] == -1690, "IR: missing NEC address complement");
    // Command 0x002A starts: 0,1,0,1,0,1,0,0...
    const size_t command_start = 2 + 16 * 2;
    require(ir.timings[command_start] == 560 && ir.timings[command_start + 1] == -560,
            "IR: command bit 0");
    require(ir.timings[command_start + 2] == 560 && ir.timings[command_start + 3] == -1690,
            "IR: command bit 1");
    require(ir.timings[command_start + 16] == 560 && ir.timings[command_start + 17] == -1690,
            "IR: missing NEC command complement");

    const auto ir_record = encode_record(ir);
    StoredSignal ir_decoded;
    error.clear();
    require(decode_record(ir_record, ir_decoded, &error), "IR record: " + error);
    require(ir_decoded.kind == SignalKind::IR && ir_decoded.timings == ir.timings,
            "IR record: decoded data differs from input");

    const std::string ir_necext =
        "Filetype: IR signals file\nVersion: 1\n#\nname: On\ntype: parsed\n"
        "protocol: NECext\naddress: 00 EF 00 00\ncommand: 02 FD 00 00\n";
    ParsedSignal ir_necext_signal;
    error.clear();
    require(parse_flipper_file(ir_necext, ir_necext_signal, error),
            "IR NECext parse: " + error);
    require(ir_necext_signal.kind == SignalKind::IR &&
                ir_necext_signal.encoding == SignalEncoding::NEC,
            "IR NECext: wrong signal kind or encoding");
    require(ir_necext_signal.source_format == "Flipper IR parsed NECext",
            "IR NECext: wrong source format");
    require(ir_necext_signal.code == 0xEF00FD02ULL,
            "IR NECext: address or command was changed");
    require(ir_necext_signal.frequency == 38000U && ir_necext_signal.duty_percent == 33U,
            "IR NECext: wrong carrier configuration");
    require(ir_necext_signal.timings.size() == 67U,
            "IR NECext: expected 67 timings");
    // Address 00 EF and command 02 FD are transmitted LSB first without
    // synthesizing the standard NEC address complement.
    require(ir_necext_signal.timings[2] == 560 && ir_necext_signal.timings[3] == -560,
            "IR NECext: address bit 0");
    require(ir_necext_signal.timings[18] == 560 && ir_necext_signal.timings[19] == -1690,
            "IR NECext: extended address bit 8");
    require(ir_necext_signal.timings[26] == 560 && ir_necext_signal.timings[27] == -560,
            "IR NECext: extended address bit 12");
    require(ir_necext_signal.timings[command_start] == 560 &&
                ir_necext_signal.timings[command_start + 1] == -560 &&
                ir_necext_signal.timings[command_start + 2] == 560 &&
                ir_necext_signal.timings[command_start + 3] == -1690,
            "IR NECext: command prefix");
    const auto ir_necext_record = encode_record(ir_necext_signal);
    StoredSignal ir_necext_decoded;
    error.clear();
    require(decode_record(ir_necext_record, ir_necext_decoded, &error),
            "IR NECext record: " + error);
    require(ir_necext_decoded.code == ir_necext_signal.code &&
                ir_necext_decoded.timings == ir_necext_signal.timings,
            "IR NECext record: decoded data differs from input");

    // Minimal tests for raw formats supported by the importer.
    const std::string ir_raw =
        "Filetype: IR signals file\nVersion: 1\n#\nname: raw_test\ntype: raw\n"
        "frequency: 38000\nduty_cycle: 0.330000\ndata: 9000 4500 560 560\n";
    ParsedSignal ir_raw_signal;
    error.clear();
    require(parse_flipper_file(ir_raw, ir_raw_signal, error), "IR RAW parse: " + error);
    require((ir_raw_signal.timings == std::vector<int32_t>{9000, -4500, 560, -560}),
            "IR RAW: invalid timing signs");

    const std::string rf_raw =
        "Filetype: Flipper SubGhz RAW File\nVersion: 1\nFrequency: 433920000\n"
        "Preset: FuriHalSubGhzPresetOok650Async\nProtocol: RAW\n"
        "RAW_Data: -500 400 -800 200 200 -12000\n";
    ParsedSignal rf_raw_signal;
    error.clear();
    require(parse_flipper_file(rf_raw, rf_raw_signal, error), "RF RAW parse: " + error);
    require((rf_raw_signal.timings == std::vector<int32_t>{400, -800, 400, -12000}),
            "RF RAW: invalid timings");

    std::cout << "OK: Princeton example parser\n";
    std::cout << "OK: Dooya example parser\n";
    std::cout << "OK: NEC example parser\n";
    std::cout << "OK: NECext parser\n";
    std::cout << "OK: int32 NVS record and RAW formats\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "ERROR: " << error.what() << '\n';
    return 1;
  }
}
