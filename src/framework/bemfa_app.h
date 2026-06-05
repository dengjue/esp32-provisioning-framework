#ifndef BEMFA_APP_H
#define BEMFA_APP_H

#include <stddef.h>
#include <stdint.h>

/**
 * Business-layer callbacks and MQTT settings.
 * Fill this struct in app_config.h / app_handlers.cpp.
 */
struct BemfaAppConfig {
  const char* mqttBroker;
  uint16_t    mqttPort;
  const char* const* mqttTopics;
  size_t      mqttTopicCount;

  uint8_t     deviceType;
  const char* deviceName;

  void (*onMqttMessage)(const char* topic, const uint8_t* payload, size_t len);
  void (*onRunning)(void);
  void (*onWifiConnected)(void);
  void (*onMqttConnected)(void);
  void (*onEnterProvisioning)(const char* reason);
};

#endif
