#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <WiFi.h>
#include "framework_config.h"
#include "ble_provisioning_manager.h"

class WiFiManager {
private:
  char deviceCode[32];
  bool connected;
  bool reconnecting;
  unsigned long reconnectStartMs;
  unsigned long lastRetryMs;
  BLEProvisioningManager* bleManager;

  void updateDeviceCode();

public:
  explicit WiFiManager(BLEProvisioningManager& bleManager);

  bool scanAndCheckSSID(const char* targetSSID);
  bool connect();
  bool isConnected();
  void beginReconnect();
  bool tickReconnect();
  bool isReconnecting() const;
  void cancelReconnect();
  void getDeviceCode(char* buffer, size_t size);
};

#endif
