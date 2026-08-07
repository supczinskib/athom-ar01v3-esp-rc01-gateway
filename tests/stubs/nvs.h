#pragma once
#include <cstddef>
#include <cstdint>
using nvs_handle_t = int;
using esp_err_t = int;
static constexpr esp_err_t ESP_OK = 0;
static constexpr esp_err_t ESP_ERR_NVS_NOT_FOUND = 1;
static constexpr int NVS_READWRITE = 0;
static constexpr int NVS_READONLY = 1;
inline esp_err_t nvs_open(const char *, int, nvs_handle_t *h) { *h=1; return ESP_OK; }
inline esp_err_t nvs_erase_key(nvs_handle_t, const char *) { return ESP_OK; }
inline esp_err_t nvs_set_blob(nvs_handle_t, const char *, const void *, size_t) { return ESP_OK; }
inline esp_err_t nvs_get_blob(nvs_handle_t, const char *, void *, size_t *) { return ESP_ERR_NVS_NOT_FOUND; }
inline esp_err_t nvs_commit(nvs_handle_t) { return ESP_OK; }
inline void nvs_close(nvs_handle_t) {}
