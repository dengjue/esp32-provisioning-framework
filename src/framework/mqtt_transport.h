#ifndef MQTT_TRANSPORT_H
#define MQTT_TRANSPORT_H

#include <PubSubClient.h>
#include <WiFi.h>
#include "bemfa_app.h"

class MqttTransport {
public:
  explicit MqttTransport(const BemfaAppConfig& app);

  void begin();
  void loop();
  void disconnect();
  bool isConnected();
  bool publish(const char* topic, const char* payload, bool retained = false);

private:
  const BemfaAppConfig& app;
  WiFiClient espClient;
  PubSubClient client;

  static MqttTransport* instance;
  unsigned long lastReconnectAttemptMs;

  void connect();
  void reconnect();
  static void callbackWrapper(char* topic, byte* payload, unsigned int length);
  void dispatchMessage(char* topic, byte* payload, unsigned int length);
};

#endif
