#include "ble_provisioning_manager.h"
#include "framework_config.h"

BLEProvisioningManager* BLEProvisioningManager::instance = nullptr;

const char* BLEProvisioningManager::SERVICE_UUID = "BEFA2000-BE35-4A5A-9C4A-BE0E02000000";
const char* BLEProvisioningManager::RX_UUID = "BEFA2001-BE35-4A5A-9C4A-BE0E02000000";
const char* BLEProvisioningManager::TX_UUID = "BEFA2002-BE35-4A5A-9C4A-BE0E02000000";

BLEProvisioningManager::BLEProvisioningManager()
  : bleServer(nullptr), txCharacteristic(nullptr), rxCharacteristic(nullptr), advertising(nullptr),
    provisioningActive(false), provisioningJustFinished(false), deviceConnected(false), txSubscribed(false),
    wifiConnectStarted(false), firstWifiConfig(false), provisionState(STATE_IDLE),
    pendingHelloSeq(-1), wifiStatusSeq(0), wifiConnectStartMs(0), wifiLastNotifyMs(0), wifiLastRetryMs(0),
    magicNumber(0), deviceType(DEVICE_TYPE), currentConnInfo(nullptr), lastBleNotifyMs(0) {
  memset(wifiSSID, 0, sizeof(wifiSSID));
  memset(wifiPassword, 0, sizeof(wifiPassword));
  safeCopy(deviceName, sizeof(deviceName), DEVICE_NAME);
  instance = this;
}

void BLEProvisioningManager::setDeviceIdentity(uint8_t type, const char* name) {
  deviceType = type;
  safeCopy(deviceName, sizeof(deviceName), name);
}

void BLEProvisioningManager::begin() {
  prefs.begin("wifi_config", false);
  magicNumber = prefs.getUChar("magic", 0);
  prefs.getString("ssid", "").toCharArray(wifiSSID, sizeof(wifiSSID));
  prefs.getString("password", "").toCharArray(wifiPassword, sizeof(wifiPassword));
}

void BLEProvisioningManager::loop() {
  if (!provisioningActive) return;

  if (pendingHelloSeq >= 0 && txSubscribed) {
    sendHello(pendingHelloSeq);
    pendingHelloSeq = -1;
  }

  if (!wifiConnectStarted || provisionState != STATE_WIFI_CONNECTING) {
    return;
  }

  unsigned long now = millis();

  if (now - wifiLastNotifyMs > 3000) {
    wifiLastNotifyMs = now;
    sendStatus(wifiStatusSeq, 0, "wifi_connecting", 40);
  }

  if (WiFi.status() != WL_CONNECTED && now - wifiLastRetryMs > WIFI_RETRY_INTERVAL_MS) {
    wifiLastRetryMs = now;
    Serial.println("[BLE] WiFi retry begin");
    WiFi.disconnect();
    delay(100);
    WiFi.begin(wifiSSID, wifiPassword);
  }

  if (WiFi.status() == WL_CONNECTED) {
    wifiConnectStarted = false;
    provisionState = STATE_WIFI_CONNECTED;
    Serial.printf("[BLE] WiFi connected, IP: %s\n", WiFi.localIP().toString().c_str());
    sendStatus(wifiStatusSeq, 0, "wifi_connected", 100);
    delay(WIFI_CONNECTED_TO_DONE_GAP_MS);

    saveWiFiCredentials(wifiSSID, wifiPassword);
    sendStatus(wifiStatusSeq, 0, "done", 100);
    delay(DONE_SETTLE_MS);

    provisionState = STATE_DONE;
    firstWifiConfig = false;
    if (advertising != nullptr) {
      advertising->stop();
    }
    stopProvisioning();
  } else if (now - wifiConnectStartMs > WIFI_CONNECT_TIMEOUT_MS) {
    provisionState = STATE_FAILED;
    wifiConnectStarted = false;
    sendStatus(wifiStatusSeq, 1003, "wifi_failed", 100, "wifi_timeout", true);
  }
}

bool BLEProvisioningManager::isProvisioningMode() {
  return provisioningActive;
}

bool BLEProvisioningManager::isProvisioningSessionActive() const {
  if (!provisioningActive) {
    return false;
  }
  if (deviceConnected) {
    return true;
  }
  switch (provisionState) {
    case STATE_WIFI_RECEIVED:
    case STATE_WIFI_CONNECTING:
    case STATE_WIFI_CONNECTED:
      return true;
    default:
      return false;
  }
}

bool BLEProvisioningManager::hasWiFiConfig() {
  return magicNumber == MAGIC && strlen(wifiSSID) > 0;
}

bool BLEProvisioningManager::hasProvisioningJustFinished() {
  if (provisioningJustFinished) {
    provisioningJustFinished = false;
    return true;
  }
  return false;
}

void BLEProvisioningManager::startProvisioning() {
  if (provisioningActive) return;

  provisionState = STATE_IDLE;
  wifiConnectStarted = false;
  firstWifiConfig = false;
  pendingHelloSeq = -1;
  wifiStatusSeq = 0;
  wifiConnectStartMs = 0;
  wifiLastNotifyMs = 0;
  wifiLastRetryMs = 0;
  initBLE();
  provisioningActive = true;
  Serial.println("[BLE] Provisioning mode started");
}

void BLEProvisioningManager::stopProvisioning(bool notifyFinished) {
  if (!provisioningActive) return;

  Serial.println("[BLE] Stopping provisioning...");

  if (bleServer && deviceConnected && currentConnInfo) {
    Serial.println("[BLE] Disconnecting device first...");
    bleServer->disconnect(*currentConnInfo);
    delay(200);
  }

  deinitBLE();
  provisioningActive = false;
  if (notifyFinished) {
    provisioningJustFinished = true;
  }
  Serial.println("[BLE] Provisioning mode stopped");
}

void BLEProvisioningManager::getWiFiCredentials(char* ssid, size_t ssidSize, char* password, size_t passwordSize) {
  safeCopy(ssid, ssidSize, wifiSSID);
  safeCopy(password, passwordSize, wifiPassword);
}

void BLEProvisioningManager::saveWiFiCredentials(const char* ssid, const char* password) {
  safeCopy(wifiSSID, sizeof(wifiSSID), ssid);
  safeCopy(wifiPassword, sizeof(wifiPassword), password);
  prefs.putUChar("magic", MAGIC);
  prefs.putString("ssid", wifiSSID);
  prefs.putString("password", wifiPassword);
  magicNumber = MAGIC;
  Serial.println("[BLE] WiFi credentials saved");
}

void BLEProvisioningManager::clearConfig() {
  prefs.clear();
  magicNumber = 0;
  memset(wifiSSID, 0, sizeof(wifiSSID));
  memset(wifiPassword, 0, sizeof(wifiPassword));
  Serial.println("[BLE] Config cleared");
}

void BLEProvisioningManager::initBLE() {
  uint16_t shortCode = makeShortCode();
  String bleName = "Bemfa_" + String(shortCode, HEX);

  NimBLEDevice::init(bleName.c_str());
  NimBLEDevice::setMTU(185);

  bleServer = NimBLEDevice::createServer();
  bleServer->setCallbacks(new ServerCallbacks());

  NimBLEService* service = bleServer->createService(SERVICE_UUID);

  txCharacteristic = service->createCharacteristic(TX_UUID, NIMBLE_PROPERTY::NOTIFY);
  txCharacteristic->setCallbacks(new TxCallbacks());

  rxCharacteristic = service->createCharacteristic(RX_UUID, NIMBLE_PROPERTY::WRITE);
  rxCharacteristic->setCallbacks(new RxCallbacks());

  service->start();
  advertising = NimBLEDevice::getAdvertising();

  NimBLEAdvertisementData advData;
  NimBLEAdvertisementData scanRespData;

  advData.setFlags(0x06);
  scanRespData.setName(bleName.c_str());
  scanRespData.setCompleteServices(NimBLEUUID(SERVICE_UUID));

  uint8_t manufacturerField[11] = {
      10,
      0xFF,
      0xFA, 0xBE,
      0x02, 0x00,
      deviceType,
      1,
      0x01,
      (uint8_t)(shortCode & 0xFF),
      (uint8_t)((shortCode >> 8) & 0xFF)
  };
  advData.addData(manufacturerField, sizeof(manufacturerField));

  advertising->setAdvertisementData(advData);
  advertising->setScanResponseData(scanRespData);
  advertising->setMaxInterval(100);
  advertising->setMinInterval(100);
  advertising->start();

  Serial.print("[BLE] Advertising started, name: ");
  Serial.println(bleName);
}

void BLEProvisioningManager::deinitBLE() {
  if (advertising) {
    advertising->stop();
  }
  NimBLEDevice::deinit(true);
  bleServer = nullptr;
  txCharacteristic = nullptr;
  rxCharacteristic = nullptr;
  advertising = nullptr;
  deviceConnected = false;
  txSubscribed = false;
}

void BLEProvisioningManager::safeCopy(char* dest, size_t size, const char* src) {
  if (size == 0) return;
  if (src == nullptr) {
    dest[0] = '\0';
    return;
  }
  strncpy(dest, src, size - 1);
  dest[size - 1] = '\0';
}

String BLEProvisioningManager::getBaseMacString() {
  uint64_t chipId = ESP.getEfuseMac();
  char mac[13];
  snprintf(mac, sizeof(mac), "%02X%02X%02X%02X%02X%02X",
           (uint8_t)(chipId >> 40), (uint8_t)(chipId >> 32), (uint8_t)(chipId >> 24),
           (uint8_t)(chipId >> 16), (uint8_t)(chipId >> 8), (uint8_t)chipId);
  return String(mac);
}

uint16_t BLEProvisioningManager::makeShortCode() {
  String mac = getBaseMacString();
  long value = strtol(mac.substring(8).c_str(), nullptr, 16);
  return (uint16_t)(value & 0xFFFF);
}

void BLEProvisioningManager::ServerCallbacks::onConnect(NimBLEServer*, NimBLEConnInfo& connInfo) {
  if (instance) {
    instance->deviceConnected = true;
    instance->provisionState = STATE_BLE_CONNECTED;
    instance->currentConnInfo = &connInfo;
    Serial.println("[BLE] Device connected");
  }
}

void BLEProvisioningManager::ServerCallbacks::onDisconnect(NimBLEServer* server, NimBLEConnInfo&, int) {
  if (instance) {
    instance->deviceConnected = false;
    instance->txSubscribed = false;
    Serial.println("[BLE] Device disconnected");
    if (server && instance->provisionState != STATE_DONE) {
      delay(200);
      server->getAdvertising()->start();
    }
  }
}

void BLEProvisioningManager::TxCallbacks::onSubscribe(NimBLECharacteristic*, NimBLEConnInfo&, uint16_t subValue) {
  if (instance) {
    instance->txSubscribed = (subValue != 0);
    Serial.print("[BLE] TX subscribe: ");
    Serial.println(subValue);
    if (instance->txSubscribed && instance->pendingHelloSeq >= 0) {
      instance->sendHello(instance->pendingHelloSeq);
      instance->pendingHelloSeq = -1;
    }
  }
}

void BLEProvisioningManager::RxCallbacks::onWrite(NimBLECharacteristic* characteristic, NimBLEConnInfo&) {
  if (instance) {
    std::string value = characteristic->getValue();
    if (!value.empty()) {
      instance->handleBLECommand(value);
    }
  }
}

void BLEProvisioningManager::handleBLECommand(const std::string& value) {
  Serial.print("[BLE] Received: ");
  Serial.println(value.c_str());

  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, value.c_str());
  int seq = doc["seq"] | 0;

  if (error) {
    sendError(seq, 3002, "invalid_json", false);
    return;
  }

  const char* cmd = doc["cmd"] | "";

  if (strcmp(cmd, "hello") == 0) {
    provisionState = STATE_READY;
    pendingHelloSeq = seq;
    if (txSubscribed) {
      sendHello(seq);
      pendingHelloSeq = -1;
    }
    return;
  }

  if (strcmp(cmd, "wifi") == 0) {
    if (provisionState != STATE_READY && provisionState != STATE_FAILED) {
      sendError(seq, 3001, "invalid_state", true);
      return;
    }

    const char* ssid = doc["ssid"] | "";
    const char* password = doc["password"] | "";
    const char* token = doc["token"] | "";

    if (strlen(ssid) == 0 || strlen(token) == 0) {
      sendError(seq, 3002, "invalid_json", false);
      return;
    }

    safeCopy(wifiSSID, sizeof(wifiSSID), ssid);
    safeCopy(wifiPassword, sizeof(wifiPassword), password);
    firstWifiConfig = true;
    provisionState = STATE_WIFI_RECEIVED;
    wifiStatusSeq = seq + 1;

    StaticJsonDocument<128> ack;
    ack["cmd"] = "wifi";
    ack["seq"] = seq;
    ack["code"] = 0;
    ack["accepted"] = true;
    String out;
    serializeJson(ack, out);
    notifyJson(out);
    delay(WIFI_ACK_TO_STATUS_GAP_MS);

    sendStatus(seq + 1, 0, "received", 20);
    connectWifiAsync();
    return;
  }

  if (strcmp(cmd, "finish") == 0) {
    sendStatus(seq, 0, "done", 100);
    sendResponse("finish", seq, 0);
    provisionState = STATE_DONE;
    if (advertising != nullptr) {
      advertising->stop();
    }
    return;
  }

  sendError(seq, 3003, "unsupported_cmd", false);
}

bool BLEProvisioningManager::notifyJson(const String& payload) {
  if (!deviceConnected || txCharacteristic == nullptr || !txSubscribed) {
    Serial.println("[BLE] Notify skipped: not ready");
    return false;
  }

  unsigned long now = millis();
  if (lastBleNotifyMs != 0) {
    unsigned long elapsed = now - lastBleNotifyMs;
    if (elapsed < BLE_NOTIFY_GAP_MS) {
      delay(BLE_NOTIFY_GAP_MS - elapsed);
    }
  }

  bool notifyOk = false;
  for (int attempt = 1; attempt <= BLE_NOTIFY_RETRY_COUNT; attempt++) {
    notifyOk = txCharacteristic->notify((const uint8_t*)payload.c_str(), payload.length());
    lastBleNotifyMs = millis();
    Serial.print("[BLE] Notify: ");
    Serial.print(notifyOk ? "ok" : "fail");
    Serial.print(", attempt=");
    Serial.print(attempt);
    Serial.print(", len=");
    Serial.print(payload.length());
    Serial.print(": ");
    Serial.println(payload);
    if (notifyOk) {
      break;
    }
    delay(BLE_NOTIFY_RETRY_GAP_MS);
  }
  return notifyOk;
}

void BLEProvisioningManager::sendResponse(const char* cmd, int seq, int code) {
  StaticJsonDocument<256> doc;
  doc["cmd"] = cmd;
  doc["seq"] = seq;
  doc["code"] = code;
  String out;
  serializeJson(doc, out);
  notifyJson(out);
}

void BLEProvisioningManager::sendHello(int seq) {
  StaticJsonDocument<384> doc;
  doc["cmd"] = "hello";
  doc["seq"] = seq;
  doc["code"] = 0;
  doc["ver"] = 2;
  doc["device_id"] = getBaseMacString();
  doc["device_type"] = deviceType;
  doc["name"] = deviceName;
  JsonArray cap = doc.createNestedArray("cap");
  cap.add("wifi");
  cap.add("mqtt");
  String out;
  serializeJson(doc, out);
  notifyJson(out);
}

void BLEProvisioningManager::sendStatus(int seq, int code, const char* stage, int progress,
                                        const char* reason, bool retryable) {
  StaticJsonDocument<320> doc;
  doc["cmd"] = "status";
  doc["seq"] = seq;
  doc["code"] = code;
  doc["stage"] = stage;
  if (progress >= 0) {
    doc["progress"] = progress;
  }
  if (reason != nullptr) {
    doc["reason"] = reason;
    doc["retryable"] = retryable;
  }
  if (WiFi.status() == WL_CONNECTED) {
    doc["ip"] = WiFi.localIP().toString();
    doc["rssi"] = WiFi.RSSI();
  }
  String out;
  serializeJson(doc, out);
  notifyJson(out);
}

void BLEProvisioningManager::sendError(int seq, int code, const char* reason, bool retryable) {
  StaticJsonDocument<256> doc;
  doc["cmd"] = "error";
  doc["seq"] = seq;
  doc["code"] = code;
  doc["reason"] = reason;
  doc["retryable"] = retryable;
  String out;
  serializeJson(doc, out);
  notifyJson(out);
}

void BLEProvisioningManager::connectWifiAsync() {
  wifiConnectStarted = true;
  wifiConnectStartMs = millis();
  wifiLastNotifyMs = 0;
  wifiLastRetryMs = millis();
  provisionState = STATE_WIFI_CONNECTING;
  WiFi.disconnect(true);
  delay(100);
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSSID, wifiPassword);
  Serial.printf("[BLE] Connecting to WiFi: %s\n", wifiSSID);
}

void BLEProvisioningManager::sendCurrentStateToClient() {
  delay(100);

  if (hasWiFiConfig()) {
    bool isWiFiConnected = WiFi.status() == WL_CONNECTED;

    Serial.printf("[BLE] Sending current state, WiFi connected: %s\n", isWiFiConnected ? "yes" : "no");

    if (isWiFiConnected) {
      StaticJsonDocument<256> finishDoc;
      finishDoc["cmd"] = "finish";
      finishDoc["seq"] = 0;
      finishDoc["code"] = 0;
      String out;
      serializeJson(finishDoc, out);
      notifyJson(out);
    } else {
      StaticJsonDocument<256> statusDoc;
      statusDoc["cmd"] = "status";
      statusDoc["seq"] = 0;
      statusDoc["code"] = 0;
      statusDoc["stage"] = "configured";
      statusDoc["ssid"] = String(wifiSSID);
      String out;
      serializeJson(statusDoc, out);
      notifyJson(out);
    }
  }
}
