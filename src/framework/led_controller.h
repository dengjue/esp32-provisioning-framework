#ifndef LED_CONTROLLER_H
#define LED_CONTROLLER_H

#include <Adafruit_NeoPixel.h>
#include "framework_config.h"

typedef enum {
  LED_MODE_OFF   = 0,
  LED_MODE_ON    = 1,
  LED_MODE_BLINK = 2,
  LED_MODE_FLASH = 3
} LEDMode;

typedef struct {
  LEDMode       mode;
  uint8_t       r, g, b;
  uint16_t      brightness;
  unsigned long interval;
  int           flashCount;
} LEDState;

class LEDController {
public:
  LEDController();
  void init();
  void begin() { init(); }

  void setSolid(uint8_t r, uint8_t g, uint8_t b,
                uint16_t brightness = LED_BRIGHTNESS);
  void setOff();
  void setBlink(uint8_t r, uint8_t g, uint8_t b,
                unsigned long interval = BLINK_FAST_MS,
                uint16_t brightness = LED_BRIGHTNESS);
  void setFlash(uint8_t r, uint8_t g, uint8_t b,
                int count,
                unsigned long interval = LED_FLASH_DURATION_MS,
                uint16_t brightness = LED_BRIGHTNESS);
  void setColor(int r, int g, int b) {
    setSolid((uint8_t)r, (uint8_t)g, (uint8_t)b);
  }

  void update();
  LEDMode getMode();

private:
  Adafruit_NeoPixel rgbLed;

  LEDState      nowState;
  LEDState      lastState;
  unsigned long lastUpdateTime;
  bool          blinkOn;
  int           currentFlashCount;

  void applyState(const LEDState& state);
  void processBlinkMode();
  void processFlashMode();
};

#endif
