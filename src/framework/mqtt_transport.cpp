#include "mqtt_transport.h"

#include <Arduino.h>

#include "framework_config.h"

MqttTransport* MqttTransport::instance = nullptr;

MqttTransport::MqttTransport(const BemfaAppConfig& app)
  : app(app), client(espClient), lastReconnectAttemptMs(0) {
  instance = this;
}

void MqttTransport::begin() {
  client.setServer(app.mqttBroker, app.mqttPort);
  client.setCallback(callbackWrapper);
  client.setKeepAlive(MQTT_KEEP_ALIVE_SEC);
  Serial.println("[MQTT] Connecting to broker...");
  connect();
}

void MqttTransport::loop() {
  client.loop();

  if (!isConnected()) {
    unsigned long now = millis();
    if (now - lastReconnectAttemptMs > MQTT_RECONNECT_DELAY_MS) {
      lastReconnectAttemptMs = now;
      reconnect();
    }
  }
}

bool MqttTransport::isConnected() const {
  return client.connected();
}

void MqttTransport::connect() {
  String clientId = "ESP32Client-";
  clientId += String(random(0xFFFF), HEX);

  Serial.printf("[MQTT] Connecting to %s:%d...\n", app.mqttBroker, app.mqttPort);

  if (client.connect(clientId.c_str())) {
    Serial.println("[MQTT] Connected");
    for (size_t i = 0; i < app.mqttTopicCount; i++) {
      const char* topic = app.mqttTopics[i];
      if (topic == nullptr || topic[0] == '\0') {
        continue;
      }
      client.subscribe(topic);
      Serial.printf("[MQTT] Subscribed to topic: %s\n", topic);
    }
    if (app.onMqttConnected != nullptr) {
      app.onMqttConnected();
    }
  } else {
    Serial.printf("[MQTT] Connection failed, rc=%d\n", client.state());
    lastReconnectAttemptMs = millis();
  }
}

void MqttTransport::reconnect() {
  connect();
}

void MqttTransport::disconnect() {
  if (client.connected()) {
    client.disconnect();
    Serial.println("[MQTT] Disconnected");
  }
}

bool MqttTransport::publish(const char* topic, const char* payload, bool retained) {
  if (!isConnected()) {
    return false;
  }
  return client.publish(topic, payload, retained);
}

void MqttTransport::callbackWrapper(char* topic, byte* payload, unsigned int length) {
  if (instance != nullptr) {
    instance->dispatchMessage(topic, payload, length);
  }
}

void MqttTransport::dispatchMessage(char* topic, byte* payload, unsigned int length) {
  if (app.onMqttMessage == nullptr) {
    return;
  }
  app.onMqttMessage(topic, payload, length);
}
