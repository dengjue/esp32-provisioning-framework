#include "app_handlers.h"

#include <Arduino.h>

#include "app_config.h"
#include "framework/bemfa_framework.h"

static void handleLedColorMessage(const char* topic, const uint8_t* payload, size_t len) {
  Serial.print("[App] Message [");
  Serial.print(topic);
  Serial.print("] ");

  String message;
  for (size_t i = 0; i < len; i++) {
    message += (char)payload[i];
  }
  Serial.println(message);

  int comma1 = message.indexOf(',');
  int comma2 = message.lastIndexOf(',');

  if (comma1 != -1 && comma2 != -1 && comma1 < comma2) {
    int r = message.substring(0, comma1).toInt();
    int g = message.substring(comma1 + 1, comma2).toInt();
    int b = message.substring(comma2 + 1).toInt();
    Serial.printf("[App] Setting LED: R=%d, G=%d, B=%d\n", r, g, b);
    BemfaFramework::led().setSolid((uint8_t)r, (uint8_t)g, (uint8_t)b);
  }
}

const BemfaAppConfig& getAppConfig() {
  static const char* topics[] = { MQTT_TOPIC };
  static BemfaAppConfig config;
  static bool initialized = false;

  if (!initialized) {
    config.mqttBroker = MQTT_BROKER;
    config.mqttPort = MQTT_PORT;
    config.mqttTopics = topics;
    config.mqttTopicCount = 1;
    config.deviceType = DEVICE_TYPE;
    config.deviceName = DEVICE_NAME;
    config.onMqttMessage = handleLedColorMessage;
    config.onRunning = nullptr;
    config.onWifiConnected = nullptr;
    config.onMqttConnected = nullptr;
    config.onEnterProvisioning = nullptr;
    initialized = true;
  }

  return config;
}
