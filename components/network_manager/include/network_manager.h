#pragma once

#include <esp_err.h>

/**
 * @brief Initialize WiFi station mode, configure mDNS, and start the HTTP server.
 * * @param ssid       The target WiFi network SSID.
 * @param password   The target WiFi network password.
 * @param hostname   The desired mDNS hostname (e.g., "rhodoshield" for rhodoshield.local).
 * @return esp_err_t ESP_OK on success, or appropriate error code.
 */
esp_err_t network_manager_start(const char *ssid, const char *password, const char *hostname);