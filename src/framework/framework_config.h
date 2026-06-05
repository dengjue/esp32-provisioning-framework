#ifndef FRAMEWORK_CONFIG_H
#define FRAMEWORK_CONFIG_H

// LED hardware defaults (override in app_config.h before including this file)
#ifndef LED_PIN
#define LED_PIN 48
#endif
#ifndef LED_COUNT
#define LED_COUNT 1
#endif
#ifndef LED_BRIGHTNESS
#define LED_BRIGHTNESS 5
#endif

// LED blink timing
#define LED_FLASH_DURATION_MS 200
#define BLINK_FAST_MS         200
#define BLINK_SLOW_MS         500

// WiFi timing
#define WIFI_CONNECT_TIMEOUT        30000
#define WIFI_RECONNECT_TIMEOUT_MS   30000
#define WIFI_RECONNECT_RETRY_MS     5000
#define WIFI_PROBE_INTERVAL_MS      20000

// MQTT timing
#define MQTT_RECONNECT_DELAY_MS     5000
#define MQTT_KEEP_ALIVE_SEC         60

// BLE device identity defaults (override in app_config.h)
#ifndef DEVICE_TYPE
#define DEVICE_TYPE 2
#endif
#ifndef DEVICE_NAME
#define DEVICE_NAME "ESP32_Device"
#endif

#endif
