#pragma once

#include <esp_err.h>

/**
 * @brief Initialize the soil moisture sensor ADC unit and channel.
 * * @return esp_err_t ESP_OK on success, or error code from ADC initialization.
 */
esp_err_t soil_moisture_init(void);

/**
 * @brief Get the raw ADC value from the sensor.
 * * @return int Raw ADC reading (0 to 4095).
 */
int soil_moisture_get_raw(void);

/**
 * @brief Get the calibrated moisture percentage.
 * * @return float Percentage value between 0.0 and 100.0.
 */
float soil_moisture_get_percentage(void);