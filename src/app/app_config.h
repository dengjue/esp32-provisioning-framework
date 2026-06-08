#ifndef APP_CONFIG_H
#define APP_CONFIG_H

// Non-secret defaults; override via .env (PlatformIO) or edit here (Arduino IDE).
#ifndef DEVICE_NAME
#define DEVICE_NAME "ESP32_LED"
#endif

#include "framework/framework_config.h"

#ifndef MQTT_BROKER
#define MQTT_BROKER "your.broker.ip"
#endif
#ifndef MQTT_PORT
#define MQTT_PORT 1883
#endif
#ifndef MQTT_TOPIC
#define MQTT_TOPIC "your/topic"
#endif

#endif
