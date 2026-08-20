# floraguard

An autonomous, modular, and industrial-grade smart irrigation system powered by an ESP32 and ESP-IDF. Specifically designed to monitor and optimize water delivery for a Rhododendron based on soil moisture kinetics and microclimate atmospheric telemetry.

## Features

- **Decoupled Architecture**: Fully isolated components built under native ESP-IDF v5.2 frameworks (Soil Moisture, Network, BME280 Atmospheric Metrics, Actuators, and SNTP Time Manager).
- **Embedded file system (SPIFFS)**: The HTML/CSS/JS web dashboard is stored as a static file in a dedicated 2 MB flash partition, reducing the RAM footprint to a minimum.
- **Asynchronous web UI**: Dynamic front-end using async JavaScript `fetch()` requests to refresh metrics without page reloads, featuring real-time connection status.
- **Quantized high-precision history**: Real-time soil moisture values are mapped onto an 8-bit scale (0 to 255) to optimize RAM storage while improving graph resolution on the client side.
- **Self-healing network time**: Background FreeRTOS task ensuring automatic SNTP time resynchronization and fallback mechanisms when internet connectivity is lost.
- **Contextual color-coded logging**: Automatic localized ISO 8601 logging (`[YYYY-MM-DD HH:MM:SS]`) mirrored dynamically to the Web UI with warning levels (green for Info, yellow for Warning, red for Error).
- **Advanced Oversampling**: Analog noise reduction layer running sequential reads to smooth out capacitive sensor floating anomalies.
- **Microclimate Telemetry**: Onboard I2C BME280 integration providing live localized temperature, relative humidity, and atmospheric pressure.
- **Agricultural Controls**: Custom background FreeRTOS control loops ensuring the plant stays within safe moisture thresholds while preventing root suffocation.
- **Horticultural Night Restriction**: Localized SNTP time harvesting to enforce daylight-only watering schedules, respecting plant nutrient absorption and respiration cycles.
- **REST JSON API**: Live embedded HTTP server exposing raw and processed operational metrics over local networks.
- **mDNS Network Resolution**: Local discovery accessible via standard zero-config hostnames (`http://floraguard.local`).
- **Power-On Self-Test (POST)**: One-second diagnostic hardware sequence flashing all notification LEDs on boot.

---

## Hardware Configuration

The system utilizes an **ESP32 DevKitC V4** micro-controller. Due to layout restrictions, all active peripherals are mapped onto the fully bidirectional right-side header pins:

| Peripheral | GPIO Pin | Type | Notes |
| :--- | :---: | :---: | :--- |
| **Capacitive Moisture Sensor v1.2** | GPIO34 | Analog Input | Mapped to ADC1 Channel 6 |
| **BME280 SDA** | GPIO25 | I2C Bidirectional | Custom routed Master SDA line |
| **BME280 SCL** | GPIO26 | I2C Bidirectional | Custom routed Master SCL line |
| **Water Pump (MOSFET/Relay)** | GPIO13 | Digital Output | High-isolation push-pull output |
| **LED Blue** | GPIO14 | Digital Output | Network status and HTTP request heartbeat |
| **LED Yellow** | GPIO12 | Digital Output | Soil moisture warning indicator (Dry state) |
| **LED Red** | GPIO27 | Digital Output | Critical error / sensor failure / flooded state |

### Hardware Component - 3D Printing
The custom sub-surface delivery nozzle is located at the root of the project: `stl/Watering_stake.stl`.
- **Material**: **PETG** (mandatory for hydrophobic longevity and bio-chemical soil resistance).
- **Slicing parameters**: 4-5 perimeters for absolute water-tightness under pump head pressure, 0.20mm layer height, low cooling fan (20%), and Gyroid infill at 25%.
- **Design details**: Includes 16 angled distribution orifices ($1.5\text{mm}$) matching the internal surface area of a $6\text{mm}$ standard silicone tube to prevent pump cavitation.

---

## Software Directory Structure

```text
floraguard/
├── components/
│   ├── actuator_manager/   # GPIO Push-Pull LED and Relay handlers
│   ├── bme280_manager/     # I2C abstraction and Bosch sensor interface
│   ├── http_api_manager/   # API HTTP server handlers
│   ├── log_manager/        # Logs and keep history
│   ├── sntp_manager/       # Posix Timezone DB synchronization layer
│   ├── soil_moisture/      # Oversampled ADC analog readings
│   ├── storage_manager/    # SPIFFS partition mounting and VFS drivers
│   └── wifi_manager/       # WiFi Station and mDNS handlers
├── data/
│   └── index.html          # Lightweight asynchronous front-end dashboard
├── src/
│   └── main.c              # Core initialization and FreeRTOS scheduler
├── stl/
│   └── Watering_stake.stl  # Custom 3D printable subsurface emitter
├── config.example.ini      # Global project variable template
├── partitions.csv          # Manual partition layout definition
└── platformio.ini          # PlatformIO compilation environments
```

---

## Configuration & Secret Isolation
All parameters, calibration metrics, credentials, and geographic configurations are completely externalized from the source code.

1. Duplicate the template configuration file at the root of your project:

    ```Bash
    cp config.example.ini config.ini
    ```
2. Edit config.ini with your real network credentials and empirical calibration bounds. (Note: config.ini is strictly ignored by Git settings for security compliance).

Example of an agricultural or time config bloc inside config.ini:

```ini
[agriculture]
build_flags =
    -D SOIL_MOISTURE_CRITICAL_HIGH=65.0f
    -D SOIL_MOISTURE_CRITICAL_LOW=20.0f
    -D IRRIGATION_PUMP_ON_MS=10000
    -D IRRIGATION_PUMP_OFF_MS=50000
    -D WATER_UNTIL_SATURATION=0
    
[time]
build_flags =
    -D SNTP_TIMEZONE=\"CET-1CEST,M3.5.0,M10.5.0/3\"
    -D DAYLIGHT_START_HOUR=8
    -D DAYLIGHT_END_HOUR=18
```

---

## Compilation & Deployment
This project uses the PlatformIO IDE or Core CLI toolchain.

1. Run Sensor Calibration
To fetch your sensor's specific raw bounds (MOISTURE_ADC_MIN_WET and MOISTURE_ADC_MAX_DRY), flash the hardware in calibration mode:

    ```Bash
    pio run -e calibration -t upload && pio device monitor
    ```
    Record the values when dry in the air and wet in water, then write them into your config.ini.

2. Standard Production Mode
    To clear cache files, download managed package extensions, build your source tree, and deploy both the binary application and the web interface assets:

    - Build and upload application code:
    ```bash
    rm -rf .pio
    pio run -e floraguard -t upload
    ```
    - Build and upload filesystem assets (SPIFFS):
    ```bash
    pio run -e floraguard -t uploadfs
    ```
    - Monitor system execution:
    ```bash
    pio device monitor
    ```
    
---

## Network API Specification
Once initialized and bound to your local area network, the device responds to automated REST harvesting tasks.

### Fetch Live Status
- URL: http://floraguard.local/api/status or http://<your_assigned_ip>/api/status
- Method: GET
- Response Payload (application/json):

    ```JSON
    {
        "soil":{
            "raw":1876,
            "moisture_pct":42.07
        },
        "environment":{
            "temperature_c":27.59,
            "humidity_pct":45.70,
            "pressure_hpa":1016.99
        },
        "moisture_history": [115, 115, 114, 114, 120, 150, 255, 255],
        "logs": [
            "[INF] [2026-07-16 11:52:28] WiFi connected. Time synced.",
            "[WRN] [2026-07-16 12:00:05] Alert: Soil dry (<20.00%): 18.50%",
            "[INF] [2026-07-16 12:00:05] Daylight active. Starting a watering cycle..."
        ]
    }
    ```
### HTTP dashboard
- URL: http://floraguard.local/ or http://<your_assigned_ip>/
  
---

## License
This project is open-source and registered under the GNU GPL v3 License. Feel free to fork, adapt, and scale out for your custom botanical arrays.
