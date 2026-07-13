#include "actuator_manager.h"
#include <driver/gpio.h>
#include <esp_log.h>

static const char *TAG = "ACTUATOR_MANAGER";

esp_err_t actuator_manager_init(void)
{
    // Configure all 4 pins as push-pull outputs
    gpio_config_t io_conf = {
        .pin_bit_mask = ((1ULL << GPIO_PUMP_OUTPUT)       | 
                         (1ULL << GPIO_LED_BLUE_OUTPUT)   | 
                         (1ULL << GPIO_LED_YELLOW_OUTPUT) | 
                         (1ULL << GPIO_LED_RED_OUTPUT)),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    
    esp_err_t err = gpio_config(&io_conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure GPIO configurations.");
        return err;
    }

    // Force safe initial states (All hardware components turned OFF)
    gpio_set_level(GPIO_PUMP_OUTPUT, 0);
    gpio_set_level(GPIO_LED_BLUE_OUTPUT, 0);
    gpio_set_level(GPIO_LED_YELLOW_OUTPUT, 0);
    gpio_set_level(GPIO_LED_RED_OUTPUT, 0);

    ESP_LOGI(TAG, "Actuators initialized: Pump:27, LED_Blue:14, LED_Yellow:12, LED_Red:13.");
    return ESP_OK;
}

void actuator_set_pump(bool enable)
{
    gpio_set_level(GPIO_PUMP_OUTPUT, enable ? 1 : 0);
    ESP_LOGD(TAG, "Pump hardware line altered to: %s", enable ? "ON" : "OFF");
}

void actuator_set_led_blue(bool enable)
{
    gpio_set_level(GPIO_LED_BLUE_OUTPUT, enable ? 1 : 0);
}

void actuator_set_led_yellow(bool enable)
{
    gpio_set_level(GPIO_LED_YELLOW_OUTPUT, enable ? 1 : 0);
}

void actuator_set_led_red(bool enable)
{
    gpio_set_level(GPIO_LED_RED_OUTPUT, enable ? 1 : 0);
}