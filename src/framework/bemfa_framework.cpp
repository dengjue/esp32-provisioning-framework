#include "bemfa_framework.h"

#include <Arduino.h>
#include <WiFi.h>

#include "ble_provisioning_manager.h"
#include "framework_config.h"
#include "led_status.h"
#include "mqtt_transport.h"
#include "wifi_manager.h"

enum SystemState {
  STATE_INIT,
  STATE_CHECK_CONFIG,
  STATE_TRY_CONNECT_WIFI,
  STATE_PROVISIONING,
  STATE_CONNECTED,
  STATE_RUNNING
};

static const BemfaAppConfig* s_app = nullptr;
static LEDController s_ledController;
static BLEProvisioningManager s_bleManager;
static WiFiManager s_wifiManager(s_bleManager);
static MqttTransport* s_mqttTransport = nullptr;
static LEDStatus s_ledStatus(s_ledController);

static SystemState s_currentState = STATE_INIT;
static bool s_wifiConnected = false;
static bool s_wifiRecoveryActive = false;
static bool s_wifiConnectAttemptActive = false;
static unsigned long s_lastWifiProbeMs = 0;
static bool s_mqttWasConnected = false;

static void enterProvisioningMode(const char* reason) {
  Serial.printf("[Framework] Entering provisioning: %s\n", reason);
  if (s_app != nullptr && s_app->onEnterProvisioning != nullptr) {
    s_app->onEnterProvisioning(reason);
  }
  s_wifiManager.cancelReconnect();
  s_wifiConnectAttemptActive = false;
  s_wifiRecoveryActive = false;
  s_wifiConnected = false;
  if (s_mqttTransport != nullptr) {
    s_mqttTransport->disconnect();
  }
  s_ledStatus.setMQTTConnected(false);
  s_mqttWasConnected = false;
  s_bleManager.startProvisioning();
  s_ledStatus.setProvisioningMode(true);
  s_lastWifiProbeMs = millis();
  s_currentState = STATE_PROVISIONING;
}

static void enterRunningMode() {
  s_wifiConnected = true;
  s_wifiRecoveryActive = false;
  s_wifiConnectAttemptActive = false;
  s_wifiManager.cancelReconnect();
  s_ledStatus.setProvisioningMode(false);
  if (s_app != nullptr && s_app->onWifiConnected != nullptr) {
    s_app->onWifiConnected();
  }
  if (s_mqttTransport != nullptr) {
    s_mqttTransport->begin();
  }
  s_currentState = STATE_RUNNING;
}

void BemfaFramework::begin(const BemfaAppConfig& app) {
  s_app = &app;
  s_bleManager.setDeviceIdentity(app.deviceType, app.deviceName);
  s_mqttTransport = new MqttTransport(app);

  Serial.println("\n[Framework] Starting up...");
  s_ledController.init();
  s_bleManager.begin();
  s_currentState = STATE_CHECK_CONFIG;
}

void BemfaFramework::loop() {
  s_ledStatus.update();

  switch (s_currentState) {
    case STATE_INIT:
      s_currentState = STATE_CHECK_CONFIG;
      break;

    case STATE_CHECK_CONFIG:
      Serial.println("[Framework] Checking WiFi configuration...");
      if (s_bleManager.hasWiFiConfig()) {
        char ssid[33];
        char password[65];
        s_bleManager.getWiFiCredentials(ssid, sizeof(ssid), password, sizeof(password));

        Serial.printf("[Framework] WiFi config found for \"%s\", scanning...\n", ssid);
        if (s_wifiManager.scanAndCheckSSID(ssid)) {
          Serial.println("[Framework] Target network available, connecting...");
          s_currentState = STATE_TRY_CONNECT_WIFI;
        } else {
          enterProvisioningMode("target network not found at boot");
        }
      } else {
        enterProvisioningMode("no WiFi config at boot");
      }
      break;

    case STATE_TRY_CONNECT_WIFI:
      if (!s_wifiConnectAttemptActive) {
        s_wifiManager.beginReconnect();
        s_wifiConnectAttemptActive = true;
      }
      if (s_wifiManager.tickReconnect()) {
        Serial.println("[Framework] WiFi connected, starting MQTT...");
        enterRunningMode();
      } else if (!s_wifiManager.isReconnecting()) {
        enterProvisioningMode("WiFi connection failed");
      }
      break;

    case STATE_PROVISIONING:
      s_bleManager.loop();

      if (s_bleManager.hasProvisioningJustFinished()) {
        Serial.println("[Framework] Provisioning complete");
        s_ledStatus.notifyProvisioningSuccess();

        if (s_bleManager.hasWiFiConfig()) {
          if (WiFi.status() == WL_CONNECTED) {
            Serial.printf("[Framework] WiFi already connected, IP: %s\n",
                          WiFi.localIP().toString().c_str());
            Serial.println("[Framework] Starting MQTT...");
            enterRunningMode();
          } else {
            Serial.println("[Framework] Connecting to WiFi...");
            s_currentState = STATE_TRY_CONNECT_WIFI;
          }
        } else {
          enterProvisioningMode("provisioning finished without config");
        }
      } else if (!s_bleManager.isProvisioningSessionActive()
                 && s_bleManager.hasWiFiConfig()
                 && millis() - s_lastWifiProbeMs >= WIFI_PROBE_INTERVAL_MS) {
        s_lastWifiProbeMs = millis();
        char ssid[33];
        char password[65];
        s_bleManager.getWiFiCredentials(ssid, sizeof(ssid), password, sizeof(password));
        if (s_wifiManager.scanAndCheckSSID(ssid)) {
          Serial.println("[Framework] Saved network available, auto-connecting...");
          s_bleManager.stopProvisioning(false);
          s_ledStatus.setProvisioningMode(false);
          s_currentState = STATE_TRY_CONNECT_WIFI;
        }
      }
      break;

    case STATE_CONNECTED:
      s_currentState = STATE_RUNNING;
      break;

    case STATE_RUNNING:
      if (s_wifiManager.isConnected()) {
        if (!s_wifiConnected) {
          s_wifiConnected = true;
          s_wifiRecoveryActive = false;
        }
        if (s_mqttTransport != nullptr) {
          s_mqttTransport->loop();
          bool mqttNow = s_mqttTransport->isConnected();
          if (mqttNow && !s_mqttWasConnected) {
            s_mqttWasConnected = true;
          }
          s_ledStatus.setMQTTConnected(mqttNow);
        }
        if (s_app != nullptr && s_app->onRunning != nullptr) {
          s_app->onRunning();
        }
      } else if (s_wifiManager.isReconnecting()) {
        s_ledStatus.setMQTTConnected(false);
        s_mqttWasConnected = false;
        if (s_wifiManager.tickReconnect()) {
          Serial.println("[Framework] WiFi reconnected");
          s_wifiConnected = true;
          s_wifiRecoveryActive = false;
          if (s_mqttTransport != nullptr) {
            s_mqttTransport->loop();
            s_ledStatus.setMQTTConnected(s_mqttTransport->isConnected());
          }
        } else if (s_wifiRecoveryActive && !s_wifiManager.isReconnecting()) {
          enterProvisioningMode("reconnect timeout");
        }
      } else if (s_wifiConnected) {
        Serial.println("[Framework] WiFi disconnected, checking available networks...");
        s_wifiConnected = false;
        s_ledStatus.setMQTTConnected(false);
        s_mqttWasConnected = false;
        if (s_mqttTransport != nullptr) {
          s_mqttTransport->disconnect();
        }

        char ssid[33];
        char password[65];
        s_bleManager.getWiFiCredentials(ssid, sizeof(ssid), password, sizeof(password));

        if (s_wifiManager.scanAndCheckSSID(ssid)) {
          Serial.println("[Framework] Saved network still available, reconnecting...");
          s_wifiManager.beginReconnect();
          s_wifiRecoveryActive = true;
        } else {
          enterProvisioningMode("saved network not found");
        }
      } else if (s_wifiRecoveryActive) {
        enterProvisioningMode("reconnect timeout");
      }
      break;
  }
}

bool BemfaFramework::isWifiConnected() {
  return s_wifiManager.isConnected();
}

bool BemfaFramework::isMqttConnected() {
  return s_mqttTransport != nullptr && s_mqttTransport->isConnected();
}

bool BemfaFramework::isProvisioning() {
  return s_bleManager.isProvisioningMode();
}

bool BemfaFramework::publish(const char* topic, const char* payload, bool retained) {
  if (s_mqttTransport == nullptr) {
    return false;
  }
  return s_mqttTransport->publish(topic, payload, retained);
}

LEDController& BemfaFramework::led() {
  return s_ledController;
}
