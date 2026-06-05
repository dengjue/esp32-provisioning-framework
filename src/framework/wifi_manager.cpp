#include "wifi_manager.h"

WiFiManager::WiFiManager(BLEProvisioningManager& bleManager)
  : connected(false), reconnecting(false), reconnectStartMs(0), lastRetryMs(0),
    bleManager(&bleManager) {
  strncpy(deviceCode, "", sizeof(deviceCode) - 1);
  deviceCode[sizeof(deviceCode) - 1] = '\0';
}

bool WiFiManager::scanAndCheckSSID(const char* targetSSID) {
  if (strlen(targetSSID) == 0) {
    return false;
  }

  Serial.println("[WiFi] Scanning for available networks...");
  int numNetworks = WiFi.scanNetworks();

  if (numNetworks == 0) {
    Serial.println("[WiFi] No networks found");
    return false;
  }

  Serial.printf("[WiFi] Found %d networks\n", numNetworks);

  for (int i = 0; i < numNetworks; i++) {
    String ssid = WiFi.SSID(i);
    Serial.printf("[WiFi] %d: %s (RSSI: %d)\n", i, ssid.c_str(), WiFi.RSSI(i));
    if (ssid == targetSSID) {
      Serial.printf("[WiFi] Target network \"%s\" found!\n", targetSSID);
      return true;
    }
  }

  Serial.printf("[WiFi] Target network \"%s\" not found\n", targetSSID);
  return false;
}

void WiFiManager::updateDeviceCode() {
  uint8_t mac[6];
  WiFi.macAddress(mac);
  snprintf(deviceCode, sizeof(deviceCode), "ESP32_%02X%02X%02X%02X%02X%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  Serial.printf("[WiFi] Device code: %s\n", deviceCode);
}

bool WiFiManager::connect() {
  cancelReconnect();

  char ssid[33];
  char password[65];
  bleManager->getWiFiCredentials(ssid, sizeof(ssid), password, sizeof(password));

  if (strlen(ssid) == 0) {
    Serial.println("[WiFi] No WiFi credentials found");
    return false;
  }

  WiFi.disconnect();
  Serial.printf("[WiFi] Connecting to: %s\n", ssid);
  WiFi.begin(ssid, password);

  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED &&
         millis() - startTime < WIFI_CONNECT_TIMEOUT) {
    delay(100);
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("[WiFi] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
    updateDeviceCode();
    connected = true;
    return true;
  }

  Serial.println("[WiFi] Connection failed");
  WiFi.disconnect();
  connected = false;
  return false;
}

bool WiFiManager::isConnected() {
  return WiFi.status() == WL_CONNECTED;
}

void WiFiManager::beginReconnect() {
  char ssid[33];
  char password[65];
  bleManager->getWiFiCredentials(ssid, sizeof(ssid), password, sizeof(password));

  if (strlen(ssid) == 0) {
    Serial.println("[WiFi] No credentials for reconnect");
    return;
  }

  reconnecting = true;
  reconnectStartMs = millis();
  lastRetryMs = millis();
  connected = false;

  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.printf("[WiFi] Reconnecting to: %s\n", ssid);
}

bool WiFiManager::tickReconnect() {
  if (!reconnecting) {
    return false;
  }

  if (WiFi.status() == WL_CONNECTED) {
    reconnecting = false;
    connected = true;
    Serial.printf("[WiFi] Reconnected! IP: %s\n", WiFi.localIP().toString().c_str());
    updateDeviceCode();
    return true;
  }

  unsigned long now = millis();
  if (now - reconnectStartMs > WIFI_RECONNECT_TIMEOUT_MS) {
    reconnecting = false;
    connected = false;
    WiFi.disconnect();
    Serial.println("[WiFi] Reconnect timed out");
    return false;
  }

  if (now - lastRetryMs >= WIFI_RECONNECT_RETRY_MS) {
    lastRetryMs = now;
    char ssid[33];
    char password[65];
    bleManager->getWiFiCredentials(ssid, sizeof(ssid), password, sizeof(password));
    WiFi.disconnect();
    WiFi.begin(ssid, password);
    Serial.println("[WiFi] Reconnect retry");
  }

  return false;
}

bool WiFiManager::isReconnecting() const {
  return reconnecting;
}

void WiFiManager::cancelReconnect() {
  reconnecting = false;
}

void WiFiManager::getDeviceCode(char* buffer, size_t size) {
  strncpy(buffer, deviceCode, size - 1);
  buffer[size - 1] = '\0';
}
