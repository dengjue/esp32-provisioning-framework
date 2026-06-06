# ESP32 Provisioning Framework

[![GitHub Stars](https://img.shields.io/github/stars/dengjue/esp32-provisioning-framework?style=social)](https://github.com/dengjue/esp32-provisioning-framework)
[![GitHub License](https://img.shields.io/github/license/dengjue/esp32-provisioning-framework)](LICENSE)
[![Platform ESP32](https://img.shields.io/badge/platform-ESP32-green)](https://www.espressif.com/)
[![Arduino IDE](https://img.shields.io/badge/Arduino-IDE-green?logo=arduino)](https://www.arduino.cc/)
[![PlatformIO](https://img.shields.io/badge/PlatformIO-Framework-blue?logo=platformio)](https://platformio.org/)

ESP32 二次开发框架：巴法云 BLE 配网、MQTT 连接、LED 状态指示、断网重连与自动进入配网模式。

业务开发者只需修改 `src/app/`，主入口 `esp32-provisioning-framework.ino` 保持简洁。

## 功能

- 巴法 App BLE 配网，WiFi 凭证持久化到 Flash
- MQTT 自动连接与断线重连
- NeoPixel LED 状态指示（配网 / 连接 / 运行）
- WiFi 丢失后自动重连，失败则重新进入配网模式

## 硬件要求

| 项目 | 默认值 | 说明 |
|------|--------|------|
| 芯片 | ESP32 / ESP32-S3 | 框架本身通用；默认 LED 引脚按 S3 开发板配置 |
| LED 引脚 | GPIO 48 | 在 `app_config.h` 中 `#define LED_PIN` 覆盖 |
| LED 数量 | 1 | `#define LED_COUNT` |
| 串口波特率 | 115200 | — |

> 若使用 ESP32 经典版（DevKit），请将 `LED_PIN` 改为实际接线引脚（如 GPIO 2），PlatformIO 的 `esp32dev` 环境已内置该覆盖。

## 目录结构

```
esp32-provisioning-framework/
├── esp32-provisioning-framework.ino   # 入口（通常不改）
├── platformio.ini                     # 可选，供 PlatformIO / Cursor 使用
├── LICENSE
├── .workbuddy/skills/                 # AI 辅助开发 skill 文档
│   ├── esp32-bemfa-ble-provisioning/  # 巴法 BLE 配网协议
│   └── led-nonblocking/               # 非阻塞 LED 控制器
└── src/
    ├── framework/                     # 框架层（基础设施）
    │   ├── bemfa_framework.*          # 核心状态机与生命周期
    │   ├── bemfa_app.h                # 业务回调接口
    │   ├── ble_provisioning_manager.*
    │   ├── wifi_manager.*
    │   ├── mqtt_transport.*           # 纯 MQTT 传输层
    │   ├── led_controller.*
    │   ├── led_status.*
    │   └── framework_config.h         # 框架默认参数
    └── app/                           # 业务层（你来改这里）
        ├── app_config.h               # MQTT、设备名、引脚覆盖
        ├── app_config.h.example       # 配置模板
        └── app_handlers.cpp           # onMqttMessage 等业务逻辑
```

## 快速开始

### 方式 A：Arduino IDE（推荐新手）

1. 安装 [Arduino IDE 2.x](https://www.arduino.cc/en/software)
2. **文件 → 首选项 → 附加开发板管理器网址**，添加：
   ```
   https://espressif.github.io/arduino-esp32/package_esp32_index.json
   ```
3. **工具 → 开发板 → 开发板管理器**，搜索并安装 **esp32 by Espressif Systems**
4. **工具 → 管理库**，安装依赖：
   - [NimBLE-Arduino](https://github.com/h2zero/NimBLE-Arduino)
   - [PubSubClient](https://github.com/knolleary/pubsubclient)
   - [Adafruit NeoPixel](https://github.com/adafruit/Adafruit_NeoPixel)
   - [ArduinoJson](https://arduinojson.org/)
5. 用 Arduino IDE 打开本文件夹（含 `.ino` 的目录）
6. 复制 `src/app/app_config.h.example` 为参考，编辑 `src/app/app_config.h` 填入 MQTT 地址和 Topic
7. 选择开发板（如 **ESP32S3 Dev Module**）和串口，点击上传
8. 烧录后用 [巴法 App](https://bemfa.com/) 完成 BLE 配网

### 方式 B：PlatformIO（Cursor / VS Code）

1. 安装 [PlatformIO IDE](https://platformio.org/) 扩展
2. 用 VS Code / Cursor 打开本文件夹（根目录含 `platformio.ini`）
3. 编辑 `src/app/app_config.h`
4. 底部选择环境（默认 `esp32-s3-devkitc-1`）和串口，点击 **Upload**

## BLE 配网步骤

1. 设备首次启动或无可用 WiFi 时，LED **蓝色持续闪烁**，表示进入配网模式
2. 手机打开 **巴法 App**，添加设备，选择 BLE 配网
3. 按 App 提示连接设备蓝牙，填写 WiFi 名称和密码
4. 配网成功后 LED **绿色快速闪 3 次**；MQTT 连接成功后 **蓝色快速闪 3 次**

## 二次开发指南

### 修改 MQTT 配置

编辑 `src/app/app_config.h`（可参考 `app_config.h.example`）：

```cpp
#define MQTT_BROKER "your.broker.ip"
#define MQTT_PORT   1883
#define MQTT_TOPIC  "your/topic"
```

### 处理 MQTT 消息

在 `src/app/app_handlers.cpp` 的 `handleLedColorMessage`（或新建处理函数）中编写逻辑，并在 `getAppConfig()` 里注册到 `.onMqttMessage`。

### 周期性业务逻辑

在 `getAppConfig()` 中设置 `.onRunning` 回调，框架会在 WiFi 已连接且处于运行态时每帧调用。

### 发布 MQTT 消息

```cpp
BemfaFramework::publish("your/topic", "payload");
```

### 查询状态

```cpp
BemfaFramework::isWifiConnected();
BemfaFramework::isMqttConnected();
BemfaFramework::isProvisioning();
BemfaFramework::led().setSolid(255, 0, 0);
```

## LED 状态说明

| 状态 | LED 表现 |
|------|----------|
| 配网模式 | 蓝色持续闪烁 |
| 配网成功 | 绿色快速闪 3 次后熄灭 |
| MQTT 已连接 | 蓝色快速闪 3 次后熄灭 |

## BemfaAppConfig 回调

| 回调 | 触发时机 |
|------|----------|
| `onMqttMessage` | 收到已订阅 Topic 的消息 |
| `onRunning` | 运行态每帧（WiFi 已连接） |
| `onWifiConnected` | WiFi 首次进入运行态 |
| `onMqttConnected` | MQTT 连接成功 |
| `onEnterProvisioning` | 进入 BLE 配网模式 |

## 框架行为

- 启动时检查 Flash 中的 WiFi 配置
- 有配置则扫描目标 SSID，存在则连接，否则进入配网
- 运行中 WiFi 断开：先尝试重连，超时或 SSID 消失则自动进入配网
- 配网空闲时周期性探测已保存 WiFi，恢复后自动连接

## 依赖库

- [NimBLE-Arduino](https://github.com/h2zero/NimBLE-Arduino) ^2.1
- [PubSubClient](https://github.com/knolleary/pubsubclient) ^2.8
- [Adafruit NeoPixel](https://github.com/adafruit/Adafruit_NeoPixel) ^1.12
- [ArduinoJson](https://arduinojson.org/) ^7.0

## License

MIT — 见 [LICENSE](LICENSE)。

