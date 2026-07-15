// Copyright (c) 2026 C. Mames - Licensed under the GNU GPL v3
#include "wifi_manager.h"
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include "log_manager.h"
#include <nvs_flash.h>
#include <mdns.h>

static const char *TAG = "WIFI_MANAGER";

// Event group bits for WiFi connection tracking
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
static EventGroupHandle_t s_wifi_event_group;
static int s_retry_num = 0;

// WiFi event handler
static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < WIFI_MAX_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            LOG_WARN(TAG, "Retrying connection to the AP (%d/%d)", s_retry_num, WIFI_MAX_RETRY);
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        LOG_INFO(TAG, "Successfully assigned IP: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

esp_err_t wifi_start(const char *ssid, const char *password, const char *hostname)
{

    s_wifi_event_group = xEventGroupCreate();

    // 1. Initialize TCP/IP Network Interface Layer
    ESP_ERROR_CHECK(esp_netif_init());
    
    // ADD THIS LINE: Initialize the system event loop required by the WiFi drivers
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // 2. Create the default WiFi station instance
    esp_netif_create_default_wifi_sta();

    // 3. Initialize WiFi Driver configuration
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // 3. Register Event Handlers
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &instance_got_ip));

    // 4. Configure WiFi Station parameters
    wifi_config_t wifi_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    strncpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
    strncpy((char*)wifi_config.sta.password, password, sizeof(wifi_config.sta.password));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    LOG_INFO(TAG, "WiFi station initialization completed. Connecting to AP...");

    // Block execution until connection is established or permanently failed
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE, pdFALSE, portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        LOG_INFO(TAG, "Connected to AP SSID: %s", ssid);
        // 5. Initialize and configure mDNS service
        ESP_ERROR_CHECK(mdns_init());
        ESP_ERROR_CHECK(mdns_hostname_set(hostname));
        LOG_INFO(TAG, "mDNS responder started. Device accessible via: http://%s.local", hostname);
        mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);
        return ESP_OK;
    } else if (bits & WIFI_FAIL_BIT) {
        LOG_ERROR(TAG, "Failed to connect to SSID: %s", ssid);
        return ESP_FAIL;
    }

    return ESP_FAIL;
}