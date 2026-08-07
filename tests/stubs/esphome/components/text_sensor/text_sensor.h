#pragma once
#include <string>
namespace esphome::text_sensor {
class TextSensor { public: void publish_state(const std::string &) {} };
}
