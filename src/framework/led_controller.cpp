#include "led_controller.h"

static void copyState(LEDState& dst, const LEDState& src) {
  dst.mode       = src.mode;
  dst.r          = src.r;
  dst.g          = src.g;
  dst.b          = src.b;
  dst.brightness = src.brightness;
  dst.interval   = src.interval;
  dst.flashCount = src.flashCount;
}

LEDController::LEDController()
  : rgbLed(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800) {
  nowState  = { LED_MODE_OFF, 0, 0, 0, LED_BRIGHTNESS, BLINK_FAST_MS, -1 };
  copyState(lastState, nowState);
  lastUpdateTime    = 0;
  blinkOn           = false;
  currentFlashCount = 0;
}

void LEDController::init() {
  rgbLed.begin();
  rgbLed.setBrightness(LED_BRIGHTNESS);
  rgbLed.clear();
  rgbLed.show();
}

void LEDController::applyState(const LEDState& state) {
  rgbLed.setBrightness(state.brightness);
  rgbLed.setPixelColor(0, rgbLed.Color(state.r, state.g, state.b));
  rgbLed.show();
}

void LEDController::setSolid(uint8_t r, uint8_t g, uint8_t b, uint16_t brightness) {
  nowState.mode = LED_MODE_ON;
  nowState.r = r; nowState.g = g; nowState.b = b;
  nowState.brightness = brightness;
  copyState(lastState, nowState);
  applyState(nowState);
}

void LEDController::setOff() {
  nowState.mode = LED_MODE_OFF;
  nowState.r = nowState.g = nowState.b = 0;
  copyState(lastState, nowState);
  applyState(nowState);
}

void LEDController::setBlink(uint8_t r, uint8_t g, uint8_t b,
                              unsigned long interval, uint16_t brightness) {
  nowState.mode       = LED_MODE_BLINK;
  nowState.r = r; nowState.g = g; nowState.b = b;
  nowState.interval   = interval;
  nowState.brightness = brightness;
  nowState.flashCount = -1;
  copyState(lastState, nowState);
  lastUpdateTime = millis();
  blinkOn = true;
  applyState(nowState);
}

void LEDController::setFlash(uint8_t r, uint8_t g, uint8_t b,
                              int count, unsigned long interval, uint16_t brightness) {
  copyState(lastState, nowState);

  nowState.mode       = LED_MODE_FLASH;
  nowState.r = r; nowState.g = g; nowState.b = b;
  nowState.interval   = interval;
  nowState.brightness = brightness;
  nowState.flashCount = count * 2;

  lastUpdateTime    = millis();
  blinkOn           = true;
  currentFlashCount = 0;

  rgbLed.setBrightness(nowState.brightness);
  rgbLed.setPixelColor(0, rgbLed.Color(r, g, b));
  rgbLed.show();
}

void LEDController::processBlinkMode() {
  unsigned long now = millis();
  if (now - lastUpdateTime >= nowState.interval) {
    lastUpdateTime = now;
    blinkOn = !blinkOn;
    if (blinkOn) {
      rgbLed.setBrightness(nowState.brightness);
      rgbLed.setPixelColor(0, rgbLed.Color(nowState.r, nowState.g, nowState.b));
    } else {
      rgbLed.clear();
    }
    rgbLed.show();
  }
}

void LEDController::processFlashMode() {
  unsigned long now = millis();
  if (now - lastUpdateTime >= nowState.interval) {
    lastUpdateTime = now;
    if (blinkOn) {
      rgbLed.setBrightness(nowState.brightness);
      rgbLed.setPixelColor(0, rgbLed.Color(nowState.r, nowState.g, nowState.b));
    } else {
      rgbLed.clear();
    }
    rgbLed.show();
    blinkOn = !blinkOn;
    currentFlashCount++;

    if (currentFlashCount >= nowState.flashCount) {
      copyState(nowState, lastState);
      applyState(nowState);
    }
  }
}

void LEDController::update() {
  switch (nowState.mode) {
    case LED_MODE_OFF:   break;
    case LED_MODE_ON:    break;
    case LED_MODE_BLINK: processBlinkMode(); break;
    case LED_MODE_FLASH: processFlashMode(); break;
  }
}

LEDMode LEDController::getMode() {
  return nowState.mode;
}
