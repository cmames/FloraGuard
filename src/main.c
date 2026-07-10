#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include "soil_moisture.h"
#include "network_manager.h"


#ifdef RUN_CALIBRATION_MODE
#include "calibration.h"
#endif  

static const char *TAG = "MAIN";

void rhodoshield(void) 
{
    // Initialize NVS (Required by the WiFi stack partition layer)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize soil moisture peripheral interface
    if (soil_moisture_init() != ESP_OK) {
        ESP_LOGE(TAG, "Aborting: Soil moisture layer setup failed.");
        return;
    }

    // Launch complete network stack (WiFi + mDNS + HTTP Server)
    if (network_manager_start(WIFI_SSID, WIFI_PASSWORD, MDNS_HOSTNAME) != ESP_OK) {
        ESP_LOGE(TAG, "Network layer failed to start. Device running offline.");
    }

    ESP_LOGI(TAG, "RhodoShield firmware fully operational.");

    // Simple background heartbeat task
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
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