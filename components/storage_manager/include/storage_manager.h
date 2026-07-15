// Copyright (c) 2026 C. Mames - Licensed under the GNU GPL v3
#pragma once

#include <esp_err.h>

/**
 * @brief Mount the flash storage partition (SPIFFS).
 * @return ESP_OK on success, or failure code.
 */
esp_err_t storage_init(void);