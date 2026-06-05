#ifndef BLE_PROVISIONING_MANAGER_H
#define BLE_PROVISIONING_MANAGER_H

#include <ArduinoJson.h>
#include <NimBLEDevice.h>
#include <Preferences.h>
#include <WiFi.h>

enum ProvisionState {
  STATE_IDLE,
  STATE_BLE_CONNECTED,
  STATE_READY,
  STATE_WIFI_RECEIVED,
  STATE_WIFI_CONNECTING,
  STATE_WIFI_CONNECTED,
  STATE_DONE,
  STATE_FAILED
};

class BLEProvisioningManager {
public:
  BLEProvisioningManager();

  void begin();
  void loop();
  void setDeviceIdentity(uint8_t deviceType, const char* deviceName);

  bool isProvisioningMode();
  bool isProvisioningSessionActive() const;
  bool hasWiFiConfig();
  bool hasProvisioningJustFinished();
  void startProvisioning();
  void stopProvisioning(bool notifyFinished = true);
  void getWiFiCredentials(char* ssid, size_t ssidSize, char* password, size_t passwordSize);
  void saveWiFiCredentials(const char* ssid, const char* password);
  void clearConfig();

private:
  static BLEProvisioningManager* instance;

  Preferences prefs;

  NimBLEServer* bleServer;
  NimBLECharacteristic* txCharacteristic;
  NimBLECharacteristic* rxCharacteristic;
  NimBLEAdvertising* advertising;

  bool provisioningActive;
  bool provisioningJustFinished;
  bool deviceConnected;
  bool txSubscribed;
  bool wifiConnectStarted;
  bool firstWifiConfig;
  ProvisionState provisionState;
  int pendingHelloSeq;
  int wifiStatusSeq;
  unsigned long wifiConnectStartMs;
  unsigned long wifiLastNotifyMs;
  unsigned long wifiLastRetryMs;

  char wifiSSID[33];
  char wifiPassword[65];
  uint8_t magicNumber;
  uint8_t deviceType;
  char deviceName[32];

  NimBLEConnInfo* currentConnInfo;

  static const uint8_t MAGIC = 0xAA;
  static const char* SERVICE_UUID;
  static const char* RX_UUID;
  static const char* TX_UUID;

  static const unsigned long BLE_NOTIFY_GAP_MS = 120;
  static const unsigned long BLE_NOTIFY_RETRY_GAP_MS = 80;
  static const int BLE_NOTIFY_RETRY_COUNT = 3;
  static const unsigned long WIFI_ACK_TO_STATUS_GAP_MS = 250;
  static const unsigned long WIFI_CONNECTED_TO_DONE_GAP_MS = 250;
  static const unsigned long DONE_SETTLE_MS = 500;
  static const unsigned long WIFI_CONNECT_TIMEOUT_MS = 90000;
  static const unsigned long WIFI_RETRY_INTERVAL_MS = 5000;
  unsigned long lastBleNotifyMs;

  void initBLE();
  void deinitBLE();
  static void safeCopy(char* dest, size_t size, const char* src);
  static String getBaseMacString();
  static uint16_t makeShortCode();

  class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer*, NimBLEConnInfo&) override;
    void onDisconnect(NimBLEServer*, NimBLEConnInfo&, int) override;
  };

  class TxCallbacks : public NimBLECharacteristicCallbacks {
    void onSubscribe(NimBLECharacteristic*, NimBLEConnInfo&, uint16_t) override;
  };

  class RxCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic*, NimBLEConnInfo&) override;
  };

  void handleBLECommand(const std::string& value);
  bool notifyJson(const String& payload);
  void sendResponse(const char* cmd, int seq, int code);
  void sendHello(int seq);
  void sendStatus(int seq, int code, const char* stage, int progress,
                  const char* reason = nullptr, bool retryable = true);
  void sendError(int seq, int code, const char* reason, bool retryable);
  void connectWifiAsync();
  void sendCurrentStateToClient();
};

#endif
