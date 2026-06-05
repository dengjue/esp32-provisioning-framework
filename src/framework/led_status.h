#ifndef LED_STATUS_H
#define LED_STATUS_H

#include "led_controller.h"

class LEDStatus {
public:
  explicit LEDStatus(LEDController& led);

  void update();
  void setProvisioningMode(bool active);
  void notifyProvisioningSuccess();
  void setMQTTConnected(bool connected);

private:
  LEDController& led;
  bool provisioningMode;
  bool mqttConnected;
  bool pendingMqttConnected;
};

#endif
