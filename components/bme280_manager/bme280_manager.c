#include "bme280_manager.h"
#include <esp_log.h>
#include <driver/i2c.h>
#include <bme280.h>

static const char *TAG = "BME280_MANAGER";

// Hardware routing configurations (Right-side accessible pins)
#define I2C_MASTER_FREQ_HZ          100000

static i2c_bus_handle_t i2c_bus = NULL;
static bme280_handle_t bme280_dev = NULL;
static bool is_initialized = false;

esp_err_t bme280_manager_init(void)
{
    // 1. Configure standard I2C Master structure
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };

    // 2. Create the specialized abstraction I2C bus required by this component version
    i2c_bus = i2c_bus_create(I2C_MASTER_NUM, &conf);
    if (i2c_bus == NULL) {
        ESP_LOGE(TAG, "Failed to create I2C abstraction bus.");
        return ESP_FAIL;
    }

    // 3. Instantiate and initialize the BME280 device layer
    bme280_dev = bme280_create(i2c_bus, BME280_I2C_ADDRESS_DEFAULT);
    if (bme280_dev == NULL) {
        ESP_LOGE(TAG, "Failed to instantiate BME280 device instance.");
        return ESP_FAIL;
    }

    esp_err_t err = bme280_default_init(bme280_dev);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Sensor hardware self-initialization failed.");
        return err;
    }

    is_initialized = true;
    ESP_LOGI(TAG, "BME280 component driver registered on pins 25/26 successfully.");
    return ESP_OK;
}

esp_err_t bme280_manager_read(float *temperature, float *humidity, float *pressure)
{
    if (!is_initialized || bme280_dev == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    // Execute direct component API measurement read operations
    if (temperature) {
        bme280_read_temperature(bme280_dev, temperature);
    }
    if (humidity) {
        bme280_read_humidity(bme280_dev, humidity);
    }
    if (pressure) {
        bme280_read_pressure(bme280_dev, pressure);
    }

    return ESP_OK;
}