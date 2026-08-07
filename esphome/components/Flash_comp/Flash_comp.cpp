#include "Flash_comp.h"

namespace esphome {
namespace flash_component {

void Flash_comp::setup() {
  const esp_err_t err = nvs_flash_init();
  if (err != ESP_OK && err != ESP_ERR_NVS_NO_FREE_PAGES && err != ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_LOGE("flash_comp", "NVS initialization returned %d", err);
  }
}

bool Flash_comp::clear_signal_by_index(int index) {
  char key[16];
  std::snprintf(key, sizeof(key), "irsig_%d", index);
  nvs_handle_t handle;
  esp_err_t err = nvs_open("storage", NVS_READWRITE, &handle);
  if (err != ESP_OK) return false;
  err = nvs_erase_key(handle, key);
  if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
    nvs_close(handle);
    return false;
  }
  err = nvs_commit(handle);
  nvs_close(handle);
  return err == ESP_OK;
}

}  // namespace flash_component
}  // namespace esphome
