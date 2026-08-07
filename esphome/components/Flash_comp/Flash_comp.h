#pragma once

#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include <nvs.h>
#include <nvs_flash.h>
#include <cstdio>
#include <type_traits>
#include <vector>

namespace esphome {
namespace flash_component {

class Flash_comp : public Component {
 public:
  void setup() override;

  template<typename T> bool save_to_nvs(int index, const std::vector<T> &data) {
    static_assert(std::is_trivially_copyable<T>::value, "NVS vector type must be trivially copyable");
    char key[16];
    std::snprintf(key, sizeof(key), "irsig_%d", index);
    nvs_handle_t handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &handle);
    if (err != ESP_OK) {
      ESP_LOGE("flash_comp", "Failed to open NVS handle: %d", err);
      return false;
    }

    err = nvs_erase_key(handle, key);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
      ESP_LOGE("flash_comp", "Failed to erase old key %s: %d", key, err);
      nvs_close(handle);
      return false;
    }
    if (!data.empty()) {
      err = nvs_set_blob(handle, key, data.data(), data.size() * sizeof(T));
      if (err != ESP_OK) {
        ESP_LOGE("flash_comp", "Failed to write key %s: %d", key, err);
        nvs_close(handle);
        return false;
      }
    }
    err = nvs_commit(handle);
    nvs_close(handle);
    if (err != ESP_OK) {
      ESP_LOGE("flash_comp", "Failed to commit key %s: %d", key, err);
      return false;
    }
    ESP_LOGI("flash_comp", "Saved %u elements (%u bytes) to %s",
             (unsigned) data.size(), (unsigned) (data.size() * sizeof(T)), key);
    return true;
  }

  template<typename T> std::vector<T> load_from_nvs(int index) {
    static_assert(std::is_trivially_copyable<T>::value, "NVS vector type must be trivially copyable");
    char key[16];
    std::snprintf(key, sizeof(key), "irsig_%d", index);
    std::vector<T> data;
    nvs_handle_t handle;
    esp_err_t err = nvs_open("storage", NVS_READONLY, &handle);
    if (err != ESP_OK) return data;

    size_t required_size = 0;
    err = nvs_get_blob(handle, key, nullptr, &required_size);
    if (err == ESP_OK && required_size > 0) {
      if ((required_size % sizeof(T)) != 0) {
        ESP_LOGE("flash_comp", "Invalid blob size %u for key %s", (unsigned) required_size, key);
        nvs_close(handle);
        return data;
      }
      data.resize(required_size / sizeof(T));
      err = nvs_get_blob(handle, key, data.data(), &required_size);
      if (err != ESP_OK) data.clear();
    } else if (err != ESP_ERR_NVS_NOT_FOUND) {
      ESP_LOGE("flash_comp", "Failed to read key %s: %d", key, err);
    }
    nvs_close(handle);
    return data;
  }

  bool clear_signal_by_index(int index);
};

}  // namespace flash_component
}  // namespace esphome
