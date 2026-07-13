#include "network_manager.h"
#include "soil_moisture.h"
#include "bme280_manager.h"
#include "actuator_manager.h"
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <mdns.h>
#include <esp_http_server.h>

static const char *TAG = "NET_MANAGER";

// Event group bits for WiFi connection tracking
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
static EventGroupHandle_t s_wifi_event_group;
static int s_retry_num = 0;

static httpd_handle_t server_handle = NULL;

// WiFi event handler
static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < WIFI_MAX_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGW(TAG, "Retrying connection to the AP (%d/%d)", s_retry_num, WIFI_MAX_RETRY);
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Successfully assigned IP: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

// HTTP GET handler for URI: /api/status
static esp_err_t status_get_handler(httpd_req_t *req)
{
    actuator_set_led_blue(false);
    char json_response[128];
    
    // Fetch live data from our soil_moisture component API
    int raw_adc = soil_moisture_get_raw();
    float percentage = soil_moisture_get_percentage();
    
    // Fetch weather metrics
    float temp = 0.0f, hum = 0.0f, press = 0.0f;
    bme280_manager_read(&temp, &hum, &press);
    
    // Format data into a standard JSON payload
    snprintf(json_response, sizeof(json_response),
             "{\"soil\":{\"raw\":%d,\"moisture_pct\":%.2f},\"environment\":{\"temperature_c\":%.2f,\"humidity_pct\":%.2f,\"pressure_hpa\":%.2f}}",
             raw_adc, percentage, temp, hum, press);
    // Set HTTP headers and send response
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, json_response, HTTPD_RESP_USE_STRLEN);
    actuator_set_led_blue(true);
    
    return ESP_OK;
}

// URI structure mapping for the HTTP server
static const httpd_uri_t status_uri = {
    .uri       = HTTP_API_URI,
    .method    = HTTP_GET,
    .handler   = status_get_handler,
    .user_ctx  = NULL
};

// Start the lightweight HTTP server
static httpd_handle_t start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;

    ESP_LOGI(TAG, "Starting HTTP server on port: '%d'", config.server_port);
    if (httpd_start(&server_handle, &config) == ESP_OK) {
        httpd_register_uri_handler(server_handle, &status_uri);
        return server_handle;
    }

    ESP_LOGE(TAG, "Failed to launch HTTP server.");
    return NULL;
}

esp_err_t network_manager_start(const char *ssid, const char *password, const char *hostname)
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

    ESP_LOGI(TAG, "WiFi station initialization completed. Connecting to AP...");

    // Block execution until connection is established or permanently failed
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE, pdFALSE, portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Connected to AP SSID: %s", ssid);
        actuator_set_led_blue(true);
        // 5. Initialize and configure mDNS service
        ESP_ERROR_CHECK(mdns_init());
        ESP_ERROR_CHECK(mdns_hostname_set(hostname));
        ESP_LOGI(TAG, "mDNS responder started. Device accessible via: http://%s.local", hostname);
        mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);

        // 6. Start the API webserver
        start_webserver();
        return ESP_OK;
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG, "Failed to connect to SSID: %s", ssid);
        return ESP_FAIL;
    }

    return ESP_FAIL;
}