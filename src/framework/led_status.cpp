#include "led_status.h"
#include "framework_config.h"

LEDStatus::LEDStatus(LEDController& led)
  : led(led), provisioningMode(false), mqttConnected(false),
    pendingMqttConnected(false) {
}

void LEDStatus::update() {
  LEDMode modeBefore = led.getMode();
  led.update();

  if (pendingMqttConnected
      && modeBefore == LED_MODE_FLASH
      && led.getMode() != LED_MODE_FLASH) {
    pendingMqttConnected = false;
    led.setOff();
    led.setFlash(0, 0, 255, 3, BLINK_FAST_MS);
  }
}

void LEDStatus::setProvisioningMode(bool active) {
  if (active) {
    if (provisioningMode) {
      return;
    }
    provisioningMode = true;
    mqttConnected = false;
    pendingMqttConnected = false;
    led.setBlink(0, 0, 255, BLINK_FAST_MS);
    return;
  }

  if (!provisioningMode) {
    return;
  }
  provisioningMode = false;
  pendingMqttConnected = false;
  led.setOff();
}

void LEDStatus::notifyProvisioningSuccess() {
  provisioningMode = false;
  pendingMqttConnected = false;
  led.setOff();
  led.setFlash(0, 255, 0, 3, BLINK_FAST_MS);
}

void LEDStatus::setMQTTConnected(bool connected) {
  if (!connected) {
    mqttConnected = false;
    pendingMqttConnected = false;
    return;
  }

  if (mqttConnected) {
    return;
  }
  mqttConnected = true;

  if (led.getMode() == LED_MODE_FLASH) {
    pendingMqttConnected = true;
    return;
  }

  led.setOff();
  led.setFlash(0, 0, 255, 3, BLINK_FAST_MS);
}
