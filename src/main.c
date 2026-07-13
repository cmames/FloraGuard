#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <nvs_flash.h>

#include "soil_moisture.h"
#include "network_manager.h"
#include "bme280_manager.h"
#include "actuator_manager.h"
#include "sntp_manager.h"

#ifdef RUN_CALIBRATION_MODE
#include "calibration.h"
#endif  

static const char *TAG = "MAIN";

static void automation_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Control loop automation task started.");

    while (1) {
        int raw_moisture = soil_moisture_get_raw();
        float moisture_pct = soil_moisture_get_percentage();

        // 1. Hardware sensor safety check
        if (raw_moisture <= 0 || raw_moisture >= 4095) {
            ESP_LOGE(TAG, "Critical: Soil moisture sensor fault detected (Raw: %d)!", raw_moisture);
            actuator_set_pump(false);
            actuator_set_led_yellow(false);
            actuator_set_led_red(true);
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }

        // 2. Agricultural control thresholds
        if (moisture_pct > SOIL_MOISTURE_CRITICAL_HIGH) {
            ESP_LOGW(TAG, "Warning: Soil moisture anomaly (>%.0f%%): %.2f%%", SOIL_MOISTURE_CRITICAL_HIGH, moisture_pct);
            actuator_set_pump(false);
            actuator_set_led_red(true);
            actuator_set_led_yellow(false);
        } 
        else if (moisture_pct < SOIL_MOISTURE_CRITICAL_LOW) {
            ESP_LOGW(TAG, "Alert: Soil dry (<%.0f%%): %.2f%%", SOIL_MOISTURE_CRITICAL_LOW, moisture_pct);
            actuator_set_led_red(false);
            actuator_set_led_yellow(true);

            // Conditional validation using our sntp_manager API
            if (is_daylight_hours()) {
                ESP_LOGI(TAG, "Daylight active. Starting a 1-minute watering cycle...");
                actuator_set_pump(true);
                vTaskDelay(pdMS_TO_TICKS(IRRIGATION_PUMP_ON_MS)); // watering
                
                actuator_set_pump(false);
                vTaskDelay(pdMS_TO_TICKS(IRRIGATION_PUMP_OFF_MS)); // infiltration
                continue;
            } else {
                ESP_LOGI(TAG, "Nighttime restriction active. Postponing irrigation.");
                actuator_set_pump(false);
            }
        } 
        else {
            // Safe zone
            actuator_set_pump(false);
            actuator_set_led_red(false);
            actuator_set_led_yellow(false);
        }

        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

void rhodoshield(void) 
{
    if (actuator_manager_init() != ESP_OK) {
        ESP_LOGE(TAG, "Aborting: Actuator framework setup failed.");
        return;
    }

    // Power-On Self-Test (POST) visual check
    actuator_set_led_blue(true);
    actuator_set_led_yellow(true);
    actuator_set_led_red(true);
    vTaskDelay(pdMS_TO_TICKS(1000));
    actuator_set_led_blue(false);
    actuator_set_led_yellow(false);
    actuator_set_led_red(false);

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    if (soil_moisture_init() != ESP_OK) {
        ESP_LOGE(TAG, "Aborting: Soil moisture hardware setup failed.");
        return;
    }

    if (bme280_manager_init() != ESP_OK) {
        ESP_LOGW(TAG, "BME280 setup failed. Running without atmospheric metrics.");
    }

    if (network_manager_start(WIFI_SSID, WIFI_PASSWORD, MDNS_HOSTNAME) != ESP_OK) {
        ESP_LOGE(TAG, "Network layer failed to start. Running local mode.");
    } else {
        initialize_sntp();
    }

    ESP_LOGI(TAG, "RhodoShield firmware fully operational. Launching automation scheduler...");

    xTaskCreatePinnedToCore(automation_task, "automation_task", 4096, NULL, 5, NULL, 1);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(60000));
    }
}

void app_main(void)
{
#ifdef RUN_CALIBRATION_MODE
    ESP_LOGW("ROUTER", "Calibration mode activated.");
    run_calibration();
#else
    ESP_LOGI("ROUTER", "Standard application mode starting...");
    rhodoshield();
#endif
}