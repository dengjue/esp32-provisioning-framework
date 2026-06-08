# ESP32 Provisioning Framework

[![GitHub Stars](https://img.shields.io/github/stars/dengjue/esp32-provisioning-framework?style=social)](https://github.com/dengjue/esp32-provisioning-framework)
[![GitHub License](https://img.shields.io/github/license/dengjue/esp32-provisioning-framework)](LICENSE)
[![Platform ESP32](https://img.shields.io/badge/platform-ESP32-green)](https://www.espressif.com/)
[![Arduino IDE](https://img.shields.io/badge/Arduino-IDE-green?logo=arduino)](https://www.arduino.cc/)
[![PlatformIO](https://img.shields.io/badge/PlatformIO-Framework-blue?logo=platformio)](https://platformio.org/)

ESP32 二次开发框架：巴法云 BLE 配网、MQTT 连接、LED 状态指示、断网重连与自动进入配网模式。

业务开发者只需修改 `src/app/`，主入口 `esp32-provisioning-framework.ino` 保持简洁。

## 功能

- 巴法云 BLE 配网，WiFi 凭证持久化到 Flash
- MQTT 自动连接与断线重连
- NeoPixel LED 状态指示（配网 / 连接 / 运行）
- WiFi 丢失后自动重连，失败则重新进入配网模式
- PlatformIO 支持 `.env` 注入配置，敏感信息不提交 Git

## 硬件要求

| 项目 | 默认值 | 说明 |
|------|--------|------|
| 芯片 | ESP32 / ESP32-S3 | 框架本身通用；默认 LED 引脚按 S3 开发板配置 |
| LED 引脚 | GPIO 48 | 在 `app_config.h` 或 `.env` 中 `LED_PIN` 覆盖 |
| LED 数量 | 1 | `#define LED_COUNT` |
| 串口波特率 | 115200 | ESP32-S3 通过 USB CDC 输出日志 |

> 若使用 ESP32 经典版（DevKit），请将 `LED_PIN` 改为实际接线引脚（如 GPIO 2），PlatformIO 的 `esp32dev` 环境已内置该覆盖。

## 目录结构

```
esp32-provisioning-framework/
├── esp32-provisioning-framework.ino   # 入口（通常不改）
├── platformio.ini                     # PlatformIO 配置
├── .env.example                       # 本地配置模板（复制为 .env）
├── ble.DN5U7Nlv.png                   # BLE 配网微信小程序码
├── scripts/load_env.py                # 编译前读取 .env
├── LICENSE
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
        ├── app_config.h               # 非敏感默认值
        ├── app_config.h.example       # Arduino IDE 配置模板
        └── app_handlers.cpp           # onMqttMessage 等业务逻辑
```

## 配置 MQTT（推荐：`.env`）

敏感信息（服务器地址、Topic 等）放在项目根目录的 `.env`，**不会提交到 Git**。

```bash
cp .env.example .env
```

编辑 `.env`：

```ini
MQTT_BROKER=your.broker.ip
MQTT_PORT=1883
MQTT_TOPIC=your/topic
DEVICE_NAME=ESP32_LED
```

> `.env` 在 **编译时** 注入固件。每次修改后必须重新编译并烧录，仅重启设备不会生效。

Arduino IDE 用户请直接编辑 `src/app/app_config.h`（参考 `app_config.h.example`）。

## 烧录固件

### 前置条件

- 已安装 [PlatformIO](https://platformio.org/)（Cursor / VS Code 扩展，或独立 CLI）
- ESP32 开发板通过 USB 连接到电脑
- 已完成 `.env` 配置（PlatformIO 用户）

### 查看串口

macOS 上通常为 `/dev/cu.usbmodem*` 或 `/dev/cu.usbserial*`：

```bash
ls /dev/cu.*
```

### 编译并烧录（ESP32-S3，默认环境）

在项目根目录执行：

```bash
pio run -e esp32-s3-devkitc-1 -t upload --upload-port /dev/cu.usbmodem1101
```

将 `--upload-port` 换成你实际的串口。若已安装 PlatformIO 但 `pio` 不在 PATH，可使用完整路径，例如：

```bash
~/.platformio/penv/bin/pio run -e esp32-s3-devkitc-1 -t upload --upload-port /dev/cu.usbmodem1101
```

编译时若看到 `Loaded from .env: MQTT_BROKER, MQTT_PORT, ...`，说明配置已注入。

### ESP32 经典版（DevKit）

```bash
pio run -e esp32dev -t upload --upload-port /dev/cu.usbserial-XXXX
```

### 常见问题

| 现象 | 处理 |
|------|------|
| `Resource busy` / 端口被占用 | 先 `Ctrl+C` 关闭串口监视器，再烧录 |
| 日志仍显示 `your.broker.ip` | 修改 `.env` 后未重新烧录；执行 clean + upload |
| ESP32-S3 无串口输出 | 确认 `platformio.ini` 中已启用 `ARDUINO_USB_CDC_ON_BOOT=1` |

仅重新编译（不烧录）：

```bash
pio run -e esp32-s3-devkitc-1
```

清理后完整重编：

```bash
pio run -e esp32-s3-devkitc-1 -t clean
pio run -e esp32-s3-devkitc-1 -t upload --upload-port /dev/cu.usbmodem1101
```

### Arduino IDE（可选）

1. 安装 [Arduino IDE 2.x](https://www.arduino.cc/en/software)
2. **文件 → 首选项 → 附加开发板管理器网址**，添加：
   ```
   https://espressif.github.io/arduino-esp32/package_esp32_index.json
   ```
3. **工具 → 开发板 → 开发板管理器**，安装 **esp32 by Espressif Systems**
4. **工具 → 管理库**，安装：NimBLE-Arduino、PubSubClient、Adafruit NeoPixel、ArduinoJson
5. 编辑 `src/app/app_config.h`，选择开发板与串口，点击上传

## 查看串口日志

波特率 **115200**。烧录与监视器**不能同时占用**同一串口。

### PlatformIO 串口监视器

```bash
pio device monitor -p /dev/cu.usbmodem1101 -b 115200
```

退出：`Ctrl+C`。

### 正常启动日志示例

```
[Framework] Starting up...
[Framework] Checking WiFi configuration...
[WiFi] Reconnected! IP: 192.168.x.x
[Framework] WiFi connected, starting MQTT...
[MQTT] Connecting to your.broker.ip:1883...
[MQTT] Connected
[MQTT] Subscribed to topic: your/topic
```

若无输出，按一下开发板 **RESET** 键。ESP32-S3 建议在烧录完成后再打开监视器。

## BLE 配网

设备首次启动、无 WiFi 配置或连接失败时，LED **蓝色持续闪烁**，表示进入配网模式。

### 微信小程序扫码配网

使用微信扫描下方小程序码，按提示完成 BLE 配网：

![BLE 配网微信小程序码](ble.DN5U7Nlv.png)

### 配网步骤

1. 确认设备 LED **蓝色持续闪烁**（配网模式）
2. 微信扫描上方小程序码，打开配网小程序
3. 按提示连接设备蓝牙，填写 WiFi 名称和密码
4. 配网成功：LED **绿色快速闪 3 次**
5. MQTT 连接成功：LED **蓝色快速闪 3 次**

也可使用 [巴法 App](https://bemfa.com/) 完成 BLE 配网。

## 二次开发指南

### 处理 MQTT 消息

在 `src/app/app_handlers.cpp` 的 `handleLedColorMessage`（或新建处理函数）中编写逻辑，并在 `getAppConfig()` 里注册到 `.onMqttMessage`。

默认 Topic 消息格式为 `R,G,B`（如 `255,0,0` 设为红色）。

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

