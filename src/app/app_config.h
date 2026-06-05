#ifndef APP_CONFIG_H
#define APP_CONFIG_H

// Override framework defaults before including framework_config.h
#define DEVICE_NAME "ESP32_LED"

#include "framework/framework_config.h"

// MQTT settings — change for your deployment (see app_config.h.example)
#define MQTT_BROKER "your.broker.ip"
#define MQTT_PORT   1883
#define MQTT_TOPIC  "your/topic"

#endif
