#pragma once
#include "nvs.h"
static constexpr esp_err_t ESP_ERR_NVS_NO_FREE_PAGES = 2;
static constexpr esp_err_t ESP_ERR_NVS_NEW_VERSION_FOUND = 3;
inline esp_err_t nvs_flash_init() { return ESP_OK; }
