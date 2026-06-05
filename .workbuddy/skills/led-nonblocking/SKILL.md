---
name: led-nonblocking
description: >
  Arduino ESP32 非阻塞 LED 控制器 skill。
  框架已内置 LEDController（src/framework/led_controller.*）与 LEDStatus 状态指示。
  支持常亮、常灭、无限闪烁、有限次数闪烁四种模式，完全非阻塞。
  业务层通过 BemfaFramework::led() 控灯；框架在 loop 中自动驱动 update()。
agent_created: true
triggers:
  - 非阻塞 LED
  - led controller
  - 闪烁
  - blink
  - flash
  - 状态机 LED
---

# LED 非阻塞控制器 Skill

## 背景

本 skill 提炼自 `/Users/dj/文件/PycharmProjects/arduino_all/LSS/` 项目。
目标项目：`/Users/dj/文件/PycharmProjects/arduino_all/esp32-provisioning-framework/`

框架中已实现非阻塞 `LEDController`（`src/framework/led_controller.h/cpp`）和系统状态指示 `LEDStatus`（`src/framework/led_status.h/cpp`）。引脚与亮度常量见 `src/framework/framework_config.h`，可在 `src/app/app_config.h` 中覆盖。

---

## 设计核心：双状态快照机制

```
LEDController 内部维护两个 LEDState 对象：
  nowState  — 当前正在执行的状态
  lastState — 快照（用于 FLASH 结束后恢复）
```

### LEDMode 枚举（4 种）

| 值 | 名称 | 说明 |
|----|------|------|
| 0 | LED_MODE_OFF | 常灭 |
| 1 | LED_MODE_ON | 常亮（立即写硬件） |
| 2 | LED_MODE_BLINK | 无限闪烁（update() 驱动） |
| 3 | LED_MODE_FLASH | 有限次闪烁，结束后自动恢复 lastState |

### 三类 setter 的 nowState/lastState 行为

| Setter | nowState | lastState | 备注 |
|--------|----------|-----------|------|
| `setSolid()` / `setOff()` | 写入新状态 | 同步更新 | 立即写硬件 |
| `setBlink()` | 写入新状态 | 同步更新 | update() 持续翻转 blinkOn |
| `setFlash()` | 写入 FLASH 状态 | **先快照当前 nowState** | 闪完恢复 lastState |

> `flashCount` 内部存的是 `count * 2`（亮一下 + 灭一下 = 2 步）

---

## 移植步骤

### 第一步：在 framework_config.h / app_config.h 中确认常量

框架 `src/framework/framework_config.h` 已包含 `LED_PIN`、`LED_COUNT`、`LED_BRIGHTNESS` 及闪烁时间常量。如需改引脚，在 `src/app/app_config.h` 中 **先于** `#include "framework/framework_config.h"` 定义：

```cpp
#define LED_PIN 48
#define LED_COUNT 1
#define LED_BRIGHTNESS 5
```

闪烁时间常量（框架已内置）：

```cpp
#define LED_FLASH_DURATION_MS 200
#define BLINK_FAST_MS         200
#define BLINK_SLOW_MS         500
```

### 第二步：替换 led_controller.h

完整替换为以下内容：

```cpp
/**
 * @file led_controller.h
 * @brief 非阻塞式 RGB LED 控制器（状态机版）
 * 移植自 LSS 项目，支持四种模式：常亮、常灭、无限闪烁、有限次数闪烁
 */

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
  unsigned long interval;   // 闪烁间隔（毫秒）
  int           flashCount; // 剩余步数（-1 = 无限）
} LEDState;

class LEDController {
public:
  LEDController();
  void init();

  // 常亮
  void setSolid(uint8_t r, uint8_t g, uint8_t b,
                uint16_t brightness = LED_BRIGHTNESS);
  // 常灭
  void setOff();
  // 无限闪烁
  void setBlink(uint8_t r, uint8_t g, uint8_t b,
                unsigned long interval = BLINK_FAST_MS,
                uint16_t brightness = LED_BRIGHTNESS);
  // 有限次数闪烁（结束后自动恢复上一个稳态）
  void setFlash(uint8_t r, uint8_t g, uint8_t b,
                int count,
                unsigned long interval = LED_FLASH_DURATION_MS,
                uint16_t brightness = LED_BRIGHTNESS);

  // ★ 必须在 loop() 中每帧调用
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

#endif // LED_CONTROLLER_H
```

### 第三步：替换 led_controller.cpp

完整替换为以下内容：

```cpp
/**
 * @file led_controller.cpp
 * @brief 非阻塞式 RGB LED 控制器实现
 */

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
  : rgbLed(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800)
{
  nowState  = { LED_MODE_OFF, 0, 0, 0, LED_BRIGHTNESS, BLINK_FAST_MS, -1 };
  copyState(lastState, nowState);
  lastUpdateTime   = 0;
  blinkOn          = false;
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

// ------- setters -------

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
}

void LEDController::setFlash(uint8_t r, uint8_t g, uint8_t b,
                              int count, unsigned long interval, uint16_t brightness) {
  // 先快照当前稳态，闪完后恢复它
  copyState(lastState, nowState);

  nowState.mode       = LED_MODE_FLASH;
  nowState.r = r; nowState.g = g; nowState.b = b;
  nowState.interval   = interval;
  nowState.brightness = brightness;
  nowState.flashCount = count * 2; // 亮+灭各算一步

  lastUpdateTime    = millis();
  blinkOn           = true;
  currentFlashCount = 0;

  // 第一帧立即亮起
  rgbLed.setBrightness(nowState.brightness);
  rgbLed.setPixelColor(0, rgbLed.Color(r, g, b));
  rgbLed.show();
}

// ------- update() 内部驱动 -------

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
      // 闪烁完毕，恢复 lastState
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
```

### 第四步：框架已自动驱动 update()

本框架中 `BemfaFramework::loop()` 会通过 `LEDStatus::update()` 调用 `ledController.update()`，**主 .ino 无需手动添加**。

业务层在 `src/app/app_handlers.cpp` 中通过 API 控灯：

```cpp
BemfaFramework::led().setSolid(255, 0, 0);
BemfaFramework::led().setFlash(0, 0, 255, 3);
```

---

## 使用示例

```cpp
// 常亮绿色
ledController.setSolid(0, 255, 0);

// 常灭
ledController.setOff();

// 无限快速闪烁（红色，200ms 间隔）
ledController.setBlink(255, 0, 0, BLINK_FAST_MS);

// 闪蓝色 3 次，完成后自动恢复之前的状态
ledController.setFlash(0, 0, 255, 3);

// 闪白色 5 次，用慢速间隔
ledController.setFlash(255, 255, 255, 5, BLINK_SLOW_MS);
```

---

## 注意事项

1. **`setFlash()` 恢复的是调用时的 `nowState`**，所以如果需要"闪完后变成X颜色"，
   应先 `setSolid(X色)` 确立目标状态，再调用 `setFlash()`，而不是反过来。

2. 框架在 `BemfaFramework::begin()` 中已调用 `ledController.init()`，业务层无需重复初始化。

3. 如果 `LED_COUNT > 1`，`applyState` 目前只操作 pixel 0，需要扩展为 for 循环。

---

## 相关文件位置

- 源码参考：`/Users/dj/文件/PycharmProjects/arduino_all/LSS/LEDController.h`
- 源码参考：`/Users/dj/文件/PycharmProjects/arduino_all/LSS/LEDController.cpp`
- 框架实现：`esp32-provisioning-framework/src/framework/led_controller.h/cpp`
- 状态指示：`esp32-provisioning-framework/src/framework/led_status.h/cpp`
- 配置常量：`esp32-provisioning-framework/src/framework/framework_config.h`
- 目标项目：`/Users/dj/文件/PycharmProjects/arduino_all/esp32-provisioning-framework/`
