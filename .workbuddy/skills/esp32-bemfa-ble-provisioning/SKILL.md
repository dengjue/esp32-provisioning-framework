# ESP32 巴法 BLE 蓝牙配网 Skill

> 来源：[巴法 ESP32 蓝牙配网完整教程](https://blog.csdn.net/bemfa/article/details/160495789)

## 与本框架的关系

本 skill 描述巴法 BLE 配网协议与参考实现。在 **esp32-provisioning-framework** 中，配网逻辑已封装为框架模块，一般无需从零移植：

| 协议/能力 | 框架中的实现 |
|-----------|-------------|
| BLE GATT 服务、hello/wifi/status/finish | `src/framework/ble_provisioning_manager.cpp` |
| WiFi 凭证 Flash 存储 | 同上（Preferences `wifi_config`） |
| 配网 + 运行态状态机编排 | `src/framework/bemfa_framework.cpp` |
| 设备类型与名称 | `BemfaAppConfig.deviceType` / `deviceName`（`src/app/app_config.h`） |

二次开发时优先改 `src/app/`，配网协议细节查阅下文；仅在需要扩展 BLE 命令或修改广播字段时再动 `ble_provisioning_manager.*`。

目标项目：本仓库根目录 `esp32-provisioning-framework/`

---

## 概述

通过巴法 App 的 BLE（低功耗蓝牙）为 ESP32 开发板进行 WiFi 配网，包含协议规范、通信流程、广播设计、核心命令、状态机、错误码及完整 Arduino 示例代码。

---

## 1. 蓝牙配网整体流程

核心流程 6 步：

1. ESP32 进入待配网模式，开启 BLE 广播
2. 巴法 App 搜索附近可配网设备
3. 用户选择设备并建立蓝牙连接
4. App 向设备发送 WiFi 信息和巴法云 token
5. ESP32 连接路由器并尝试连接云端
6. 设备通过 BLE 回传状态，最终结束配网

### 通信流程图

```
App                              ESP32
 |                                 |
 | 扫描蓝牙广播                     |
 |-------------------------------->|
 | 建立 BLE 连接                    |
 |-------------------------------->|
 | 发送 hello                       |
 |-------------------------------->|
 |<--------------------------------|
 | 发送 wifi                        |
 |-------------------------------->|
 |<--------------------------------|
 | 接收 status 状态                 |
 |<--------------------------------|
 |<--------------------------------|
 | 发送 finish                      |
 |-------------------------------->|
 |<--------------------------------|
```

---

## 2. BLE GATT 服务规范

设备端实现 **1 个 Service** + **2 个 Characteristic**：

| 项目 | UUID | 属性 | 方向 |
|------|------|------|------|
| Service | `BEFA2000-BE35-4A5A-9C4A-BE0E02000000` | — | — |
| RX Characteristic | `BEFA2001-BE35-4A5A-9C4A-BE0E02000000` | WRITE | App → Device |
| TX Characteristic | `BEFA2002-BE35-4A5A-9C4A-BE0E02000000` | NOTIFY | Device → App |

- **RX**：接收 App 发送的 JSON 指令
- **TX**：通知设备状态和响应结果
- App 通过固定的 Service UUID 过滤待配网设备

---

## 3. 广播包设计

### 广播名称

格式：`Bemfa_<短码>`，例如 `Bemfa_A3F2B2`

### Manufacturer Data 字段

| 字段 | 说明 |
|------|------|
| company_id | 固定 `0xBEFA` |
| proto_major | 协议主版本，固定 `0x02` |
| proto_minor | 协议次版本，固定 `0x00` |
| device_type | 设备类型 |
| setup_state | `0=已配网`，`1=待配网`，`2=维护配网` |
| capability | 能力位 |
| short_code | 设备短码 |

示例字节：`FA BE 02 00 02 01 11 F2 A3`

> 灯、插座、风扇、开关等产品在广播阶段就把 `device_type` 带出来，方便 App 端识别展示。

---

## 4. 数据格式规范

- UTF-8 编码 JSON 字符串，通过 BLE Characteristic 收发
- 通用字段：

| 字段 | 说明 |
|------|------|
| cmd | 命令名称 |
| seq | 请求序号 |
| code | 响应码，`0` 为成功 |

### 注意事项

- 字段名统一小写加下划线
- 不要发送 `null`
- 日志中不要打印 WiFi 密码和 token
- 连续发送多条 JSON 时，每条消息至少间隔 **200ms**

---

## 5. 4 个核心命令

### 5.1 hello — 握手确认

**App → Device：**
```json
{
  "cmd": "hello",
  "seq": 1,
  "ver": 2,
  "app": "behome",
  "mtu": 185
}
```

**Device → App：**
```json
{
  "cmd": "hello",
  "seq": 1,
  "code": 0,
  "ver": 2,
  "device_id": "BH_A1B2C3",
  "device_type": 2,
  "name": "BeHome Switch",
  "cap": ["wifi", "cloud"]
}
```

> 建议 `hello` 完成后才允许接收 `wifi` 命令。

### 5.2 wifi — 下发路由器信息

**App → Device：**
```json
{
  "cmd": "wifi",
  "seq": 3,
  "ssid": "Home-WiFi",
  "password": "12345678",
  "security": "auto",
  "token": "xxxxxxxxxxxxx"
}
```

**Device → App（确认接收）：**
```json
{
  "cmd": "wifi",
  "seq": 3,
  "code": 0,
  "accepted": true
}
```

### 5.3 status — 上报配网进度

**连接中：**
```json
{
  "cmd": "status",
  "seq": 4,
  "code": 0,
  "stage": "wifi_connecting",
  "progress": 40
}
```

**连接成功：**
```json
{
  "cmd": "status",
  "seq": 5,
  "code": 0,
  "stage": "wifi_connected",
  "progress": 100,
  "ip": "192.168.1.100",
  "rssi": -45
}
```

**连接失败：**
```json
{
  "cmd": "status",
  "seq": 5,
  "code": 1001,
  "stage": "wifi_failed",
  "reason": "wrong_password",
  "retryable": true
}
```

**常见 stage 值：**

| stage | 说明 |
|-------|------|
| received | 已收到配网信息 |
| wifi_connecting | 正在连接 WiFi |
| wifi_connected | WiFi 已连接 |
| wifi_failed | WiFi 连接失败 |
| cloud_connecting | 正在连接云端 |
| cloud_failed | 云端连接失败 |
| done | 配网完成 |

### 5.4 finish — 结束配网

**App → Device：**
```json
{
  "cmd": "finish",
  "seq": 7
}
```

**Device → App：**
```json
{
  "cmd": "finish",
  "seq": 7,
  "code": 0,
  "stage": "done"
}
```

> ESP32 收到 `finish` 后，应关闭配网广播，并在短时间内断开 BLE 连接。

---

## 6. ESP32 端状态机

| 状态 | 说明 |
|------|------|
| idle | 等待配网 |
| connected | BLE 已连接 |
| ready | hello 已完成 |
| wifi_received | 已收到 WiFi |
| wifi_connecting | 正在连接 WiFi |
| wifi_connected | WiFi 已连接 |
| done | 配网完成 |
| failed | 配网失败 |

### 命令约束

| 状态 | 允许命令 |
|------|----------|
| connected | `hello` |
| ready | `wifi`、`finish` |
| wifi_connected | `finish` |
| failed | `wifi`、`finish` |

状态不合法时返回：
```json
{
  "cmd": "error",
  "seq": 0,
  "code": 3001,
  "reason": "invalid_state"
}
```

---

## 7. 错误码一览

| 错误码 | reason | 含义 |
|--------|--------|------|
| 1001 | wrong_password | WiFi 密码错误 |
| 1002 | wifi_not_found | 找不到路由器 |
| 1003 | wifi_timeout | WiFi 连接超时 |
| 1005 | unsupported_wifi | 不支持该 WiFi 类型或频段 |
| 2001 | cloud_failed | 云端连接失败 |
| 3001 | invalid_state | 当前状态不允许该命令 |
| 3002 | invalid_json | JSON 格式错误 |
| 3003 | unsupported_cmd | 不支持的命令 |
| 3004 | payload_too_large | 数据过大 |
| 9001 | internal_error | 设备内部错误 |

---

## 8. ESP32 侧实现要点

### 必备能力

- BLE 广播
- GATT Server
- RX `WRITE`
- TX `NOTIFY`
- JSON 解析
- WiFi 连接
- 状态回传

### 实用建议

1. **首次上电自动进入配网模式**：设备无保存 WiFi 信息时自动进入待配网状态
2. **长按按键重新进入配网**：用户换路由器或改密码后的重新配网入口（必备功能）
3. **配网成功后关闭广播**：避免额外耗电和重复连接
4. **保存配网信息**：至少持久化 WiFi SSID、WiFi 密码、已完成配网标志

---

## 9. 示例工程下载

| 版本 | 下载链接 |
|------|----------|
| 最小 NimBLE 配网版 | [blu_NimBLE.zip](http://file.bemfa.com/zip/esp32/ble/blu_NimBLE.zip) |
| 经典 BLE 配网版 | [blu.zip](https://file.bemfa.com/zip/esp32/ble/blu.zip) |
| BLE + TCP 整合版 | [blu_tcp.zip](https://file.bemfa.com/zip/esp32/ble/blu_tcp.zip) |
| BLE + MQTT 整合版 | [blu_mqtt.zip](https://file.bemfa.com/zip/esp32/ble/blu_mqtt.zip) |

> **推荐**：优先使用 NimBLE 版本（协议更轻量、更适合项目扩展、维护成本更低）。

---

## 10. Arduino 库安装

在 Arduino IDE 中通过 `工具 → 管理库` 搜索安装：

- `ArduinoJson`
- `AceButton`
- `NimBLE-Arduino`

如果编译后空间不足，调整分区方案：
- `Tools → Partition Scheme → Minimal SPIFFS`
- 或 `Tools → Partition Scheme → Huge APP`

---

## 11. 调试常见坑

| 坑 | 说明 |
|----|------|
| JSON 连续发送过快 | App 来不及处理，每条 JSON 至少间隔 200ms |
| WiFi 密码和 token 打到日志 | 串口/云端/调试输出中不要打印敏感字段 |
| 状态机不完整 | 容易出现重复配网、异常重连、finish 提前触发 |
| 配网成功后没及时断开 BLE | 手机端认为配网未结束，设备停留在待配网状态 |
| 没有处理 WiFi 失败重试 | `wifi_failed` 后应允许重新下发 `wifi` 命令 |

---

## 12. 推荐开发顺序

1. 先把 BLE 广播和 GATT 服务搭出来
2. 跑通 `hello` 指令
3. 跑通 `wifi` 指令和 JSON 解析
4. 接入 `WiFi.begin()` 并回传 `status`
5. 配网成功后保存参数
6. 最后再接 TCP、MQTT 或设备业务逻辑

> **关键**：先把"蓝牙配网成功"链路单独跑通，再去接 MQTT，否则出问题分不清是 BLE、WiFi 还是 MQTT 的问题。

---

## 13. 完整最小示例代码

### 依赖库

- `NimBLE-Arduino`
- `ArduinoJson`
- `Preferences`

### 需修改的配置项

```cpp
String aptype = "999";       // 001插座,002灯,003风扇,005空调,006开关,009窗帘
String Name = "新设备";       // 设备昵称
String verSion = "3.1";      // 3是tcp设备端口8344,1是MQTT设备
String room = "";            // 房间，例如客厅、卧室等，默认空
int protoType = 3;           // 3是tcp设备端口8344,1是MQTT设备
int adminID = 0;             // 企业id，默认0
```

### 完整代码

```cpp
// ESP32 BeHome BLE provisioning example.
// Arduino IDE libraries:
// 1. Install "ArduinoJson" from Library Manager.
// 2. Install "NimBLE-Arduino" from Library Manager.
// 3. Use an ESP32 board package that provides WiFi, HTTPClient, Preferences.

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <NimBLEDevice.h>
#include <Preferences.h>
#include <WiFi.h>

// =========================
// 需要根据自己产品修改的配置
// =========================
String aptype = "999";       // 001插座,002灯,003风扇,005空调,006开关,009窗帘
String Name = "新设备";         // 设备昵称
String verSion = "3.1";      // 3是tcp设备端口8344,1是MQTT设备
String room = "";            // 房间，例如客厅、卧室等，默认空
int protoType = 3;           // 3是tcp设备端口8344,1是MQTT设备
int adminID = 0;             // 企业id，默认0

// BLE 配网协议里使用的 Service / RX / TX UUID
static const char *SERVICE_UUID = "BEFA2000-BE35-4A5A-9C4A-BE0E02000000";
static const char *RX_UUID = "BEFA2001-BE35-4A5A-9C4A-BE0E02000000";
static const char *TX_UUID = "BEFA2002-BE35-4A5A-9C4A-BE0E02000000";

// 使用开发板自带 BOOT 按键做恢复出厂
static const int RESET_BUTTON_PIN = 0;  // ESP32 DevKit BOOT button
static const unsigned long RESET_HOLD_MS = 5000;

// 设备当前所处的配网状态
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

// 持久化保存到 Preferences 的配置结构
struct Config {
  char stassid[33];
  char stapsw[65];
  char cuid[65];
  char ctopic[33];
  uint8_t reboot;
  uint8_t magic;
};

// Preferences 用于替代 ESP8266 示例中的 EEPROM
Preferences prefs;
Config config;
WiFiClient bemfaClient;
HTTPClient bemfaHttp;

// BLE Server 相关对象
NimBLEServer *bleServer = nullptr;
NimBLECharacteristic *txCharacteristic = nullptr;
NimBLECharacteristic *rxCharacteristic = nullptr;
NimBLEAdvertising *advertising = nullptr;

// 运行时状态变量
ProvisionState provisionState = STATE_IDLE;
bool deviceConnected = false;
bool firstWifiConfig = false;
bool configFlag = false;
bool wifiConnectStarted = false;
bool txSubscribed = false;
unsigned long wifiConnectStartMs = 0;
unsigned long wifiLastNotifyMs = 0;
unsigned long wifiLastRetryMs = 0;
unsigned long wifiDebugLastPrintMs = 0;
unsigned long resetButtonPressStartMs = 0;
unsigned long rebootCounterClearStartMs = 0;
bool rebootCounterPendingClear = false;
String topic = "";
uint16_t shortCode = 0;
int pendingHelloSeq = -1;
int wifiStatusSeq = 0;

static const uint8_t MAGIC_NUMBER = 0xAA;
static const unsigned long BLE_NOTIFY_GAP_MS = 120;
static const unsigned long WIFI_ACK_TO_STATUS_GAP_MS = 250;
static const unsigned long WIFI_CONNECTED_TO_DONE_GAP_MS = 250;
static const unsigned long DONE_SETTLE_MS = 500;
static const unsigned long WIFI_CONNECT_TIMEOUT_MS = 90000;
static const unsigned long WIFI_RETRY_INTERVAL_MS = 5000;
static const int BLE_NOTIFY_RETRY_COUNT = 3;
static const unsigned long BLE_NOTIFY_RETRY_GAP_MS = 80;
unsigned long lastBleNotifyMs = 0;

// 读取 ESP32 芯片 eFuse 中的真实 MAC 地址
String getBaseMacString() {
  uint64_t chipId = ESP.getEfuseMac();
  char mac[13];
  snprintf(
      mac,
      sizeof(mac),
      "%02X%02X%02X%02X%02X%02X",
      (uint8_t)(chipId >> 40),
      (uint8_t)(chipId >> 32),
      (uint8_t)(chipId >> 24),
      (uint8_t)(chipId >> 16),
      (uint8_t)(chipId >> 8),
      (uint8_t)chipId);
  return String(mac);
}

// 安全复制字符串到固定长度 char 数组
void safeCopy(char *dest, size_t size, const char *src) {
  if (size == 0) {
    return;
  }
  if (src == nullptr) {
    dest[0] = '\0';
    return;
  }
  strncpy(dest, src, size - 1);
  dest[size - 1] = '\0';
}

// 取 MAC 后 6 位，再拼上设备类型，作为 topic / device_id
String macSuffixTopic() {
  String mac = getBaseMacString();
  return mac.substring(6) + aptype;
}

// 取 MAC 最后 2 个字节，作为广播 short code
uint16_t makeShortCode() {
  String mac = getBaseMacString();
  long value = strtol(mac.substring(8).c_str(), nullptr, 16);
  return (uint16_t)(value & 0xFFFF);
}

// 字符串设备类型转成数值
uint8_t deviceTypeValue() {
  return (uint8_t)aptype.toInt();
}

// 上电后尽早记录重启次数
void updateBootCounterEarly() {
  memset(&config, 0, sizeof(config));
  prefs.begin("bemfa", false);
  config.magic = prefs.getUChar("magic", 0);
  config.reboot = prefs.getUChar("reboot", 0);

  if (config.magic == MAGIC_NUMBER) {
    config.reboot = config.reboot + 1;
    prefs.putUChar("reboot", config.reboot);
    Serial.print("Boot count: ");
    Serial.println(config.reboot);

    if (config.reboot >= 4) {
      restoreFactory();
    }
  }
}

// 读取已保存的配网信息
void loadConfig() {
  memset(&config, 0, sizeof(config));
  config.magic = prefs.getUChar("magic", 0);
  config.reboot = prefs.getUChar("reboot", 0);

  prefs.getString("ssid", "").toCharArray(config.stassid, sizeof(config.stassid));
  prefs.getString("psw", "").toCharArray(config.stapsw, sizeof(config.stapsw));
  prefs.getString("uid", "").toCharArray(config.cuid, sizeof(config.cuid));
  prefs.getString("topic", "").toCharArray(config.ctopic, sizeof(config.ctopic));
  configFlag = config.magic != MAGIC_NUMBER;

  rebootCounterClearStartMs = millis();
  rebootCounterPendingClear = config.magic == MAGIC_NUMBER;
}

// 稳定运行 2 秒后，把重启计数清零（非阻塞方式）
void handleBootCounterStableClear() {
  if (!rebootCounterPendingClear) {
    return;
  }
  if (millis() - rebootCounterClearStartMs < 2000) {
    return;
  }

  config.reboot = 0;
  prefs.putUChar("reboot", 0);
  rebootCounterPendingClear = false;
  Serial.println("Boot count reset");
}

// 保存配网成功后的配置到 Flash
void saveConfig() {
  config.reboot = 0;
  prefs.putUChar("magic", MAGIC_NUMBER);
  prefs.putUChar("reboot", 0);
  prefs.putString("ssid", config.stassid);
  prefs.putString("psw", config.stapsw);
  prefs.putString("uid", config.cuid);
  prefs.putString("topic", config.ctopic);
  config.magic = MAGIC_NUMBER;
}

// 恢复出厂：清空所有持久化配置并重启
void restoreFactory() {
  Serial.println("Restore factory settings");
  prefs.clear();
  memset(&config, 0, sizeof(config));
  config.reboot = 0;
  config.magic = 0;
  delay(500);
  ESP.restart();
}

// 检测 BOOT 按键是否长按 5 秒
void handleResetButton() {
  int buttonState = digitalRead(RESET_BUTTON_PIN);
  if (buttonState == LOW) {
    if (resetButtonPressStartMs == 0) {
      resetButtonPressStartMs = millis();
    }

    if (millis() - resetButtonPressStartMs >= RESET_HOLD_MS) {
      Serial.println("BOOT button long press detected");
      restoreFactory();
    }
    return;
  }

  resetButtonPressStartMs = 0;
}

// 通过 BLE Notify 向 App 发送 JSON 字符串（含最小间隔和有限重试）
bool notifyJson(const String &payload) {
  if (!deviceConnected || txCharacteristic == nullptr || !txSubscribed) {
    Serial.println("Device -> App notify skipped: not ready");
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
    notifyOk = txCharacteristic->notify(
        (const uint8_t *)payload.c_str(), payload.length());
    lastBleNotifyMs = millis();
    Serial.print("Device -> App notify=");
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

// 通用响应
void sendResponse(const char *cmd, int seq, int code) {
  StaticJsonDocument<256> doc;
  doc["cmd"] = cmd;
  doc["seq"] = seq;
  doc["code"] = code;
  String out;
  serializeJson(doc, out);
  notifyJson(out);
}

// 通用错误响应
void sendError(int seq, int code, const char *reason, bool retryable) {
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

// 状态上报
void sendStatus(int seq, int code, const char *stage, int progress,
                const char *reason = nullptr, bool retryable = true) {
  StaticJsonDocument<256> doc;
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

// hello 响应
void sendHello(int seq) {
  StaticJsonDocument<384> doc;
  doc["cmd"] = "hello";
  doc["seq"] = seq;
  doc["code"] = 0;
  doc["ver"] = 2;
  doc["device_id"] = topic;
  doc["device_type"] = deviceTypeValue();
  doc["name"] = Name;
  JsonArray cap = doc.createNestedArray("cap");
  cap.add("wifi");
  String out;
  serializeJson(doc, out);
  notifyJson(out);
}

// info 响应
void sendInfo(int seq) {
  StaticJsonDocument<384> doc;
  doc["cmd"] = "info";
  doc["seq"] = seq;
  doc["code"] = 0;
  doc["device_id"] = topic;
  doc["product_id"] = topic;
  doc["fw"] = verSion;
  doc["hw"] = "esp32";
  doc["mac"] = getBaseMacString();
  String out;
  serializeJson(doc, out);
  notifyJson(out);
}

// 异步开始连接路由器（不要在 BLE 回调里阻塞等待）
void connectWifiAsync() {
  wifiConnectStarted = true;
  wifiConnectStartMs = millis();
  wifiLastNotifyMs = 0;
  wifiLastRetryMs = millis();
  provisionState = STATE_WIFI_CONNECTING;
  WiFi.disconnect(true);
  delay(100);
  WiFi.mode(WIFI_STA);
  WiFi.begin(config.stassid, config.stapsw);
}

// 联网成功后，请求巴法云端接口创建设备主题
bool addTopicToBemfa() {
  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }

  bemfaHttp.begin(bemfaClient, "http://pro.bemfa.com/vs/web/v1/deviceAddTopic");
  bemfaHttp.addHeader("Content-Type", "application/json; charset=UTF-8");

  StaticJsonDocument<256> jsonDoc;
  jsonDoc["uid"] = config.cuid;
  jsonDoc["name"] = Name;
  jsonDoc["topic"] = topic;
  jsonDoc["type"] = protoType;
  jsonDoc["room"] = room;
  jsonDoc["adminID"] = adminID;
  jsonDoc["wifiConfig"] = 1;
  jsonDoc["unCreate"] = 1;

  String jsonString;
  serializeJson(jsonDoc, jsonString);
  int httpCode = bemfaHttp.POST(jsonString);
  String payload = bemfaHttp.getString();
  bemfaHttp.end();

  Serial.print("deviceAddTopic httpCode: ");
  Serial.println(httpCode);
  Serial.println(payload);

  if (httpCode != 200) {
    return false;
  }

  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, payload);
  if (error) {
    Serial.print("deserializeJson failed: ");
    Serial.println(error.c_str());
    return false;
  }

  int code = doc["code"] | -1;
  int resCode = doc["data"]["code"] | -1;
  return code == 0 && (resCode == 0 || resCode == 40006);
}

// 更新 BLE 广播内容
void updateAdvertisementData(uint8_t setupState) {
  if (advertising == nullptr) {
    return;
  }

  String bleName = "Bemfa_" + String(shortCode, HEX);
  NimBLEAdvertisementData advData;
  NimBLEAdvertisementData scanRespData;
  advData.setFlags(0x06);
  scanRespData.setName(bleName.c_str());
  scanRespData.setCompleteServices(NimBLEUUID(SERVICE_UUID));

  uint8_t manufacturerField[11] = {
      10,
      0xFF,
      0xFA,
      0xBE,
      0x02,
      0x00,
      deviceTypeValue(),
      setupState,
      0x01,
      (uint8_t)(shortCode & 0xFF),
      (uint8_t)((shortCode >> 8) & 0xFF),
  };
  advData.addData(manufacturerField, sizeof(manufacturerField));
  advertising->setAdvertisementData(advData);
  advertising->setScanResponseData(scanRespData);
}

// 初始化 BLE 配网服务
void startBleProvisioning() {
  shortCode = makeShortCode();
  String bleName = "Bemfa_" + String(shortCode, HEX);
  NimBLEDevice::init(bleName.c_str());
  NimBLEDevice::setMTU(185);

  bleServer = NimBLEDevice::createServer();

  class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer *server, NimBLEConnInfo &connInfo) override {
      deviceConnected = true;
      provisionState = STATE_BLE_CONNECTED;
      Serial.println("BLE connected");
    }

    void onDisconnect(NimBLEServer *server, NimBLEConnInfo &connInfo, int reason) override {
      deviceConnected = false;
      Serial.println("BLE disconnected");
      if (provisionState != STATE_DONE) {
        delay(200);
        server->getAdvertising()->start();
      }
    }
  };
  bleServer->setCallbacks(new ServerCallbacks());

  NimBLEService *service = bleServer->createService(SERVICE_UUID);
  txCharacteristic = service->createCharacteristic(
      TX_UUID,
      NIMBLE_PROPERTY::NOTIFY);

  class TxCallbacks : public NimBLECharacteristicCallbacks {
    void onSubscribe(NimBLECharacteristic *characteristic, NimBLEConnInfo &connInfo, uint16_t subValue) override {
      txSubscribed = (subValue != 0);
      Serial.print("TX subscribe state: ");
      Serial.println(subValue);
    }
  };
  txCharacteristic->setCallbacks(new TxCallbacks());

  rxCharacteristic = service->createCharacteristic(
      RX_UUID,
      NIMBLE_PROPERTY::WRITE);

  class RxCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic *characteristic, NimBLEConnInfo &connInfo) override {
      std::string value = characteristic->getValue();
      if (value.empty()) {
        return;
      }
      Serial.print("App -> Device: ");
      Serial.println(value.c_str());

      StaticJsonDocument<512> doc;
      DeserializationError error = deserializeJson(doc, value.c_str());
      int seq = doc["seq"] | 0;
      if (error) {
        sendError(seq, 3002, "invalid_json", false);
        return;
      }

      const char *cmd = doc["cmd"] | "";
      Serial.print("BLE cmd: ");
      Serial.println(cmd);

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

        const char *ssid = doc["ssid"] | "";
        const char *password = doc["password"] | "";
        const char *token = doc["token"] | "";
        if (strlen(ssid) == 0 || strlen(token) == 0) {
          sendError(seq, 3002, "invalid_json", false);
          return;
        }

        safeCopy(config.stassid, sizeof(config.stassid), ssid);
        safeCopy(config.stapsw, sizeof(config.stapsw), password);
        safeCopy(config.cuid, sizeof(config.cuid), token);
        safeCopy(config.ctopic, sizeof(config.ctopic), topic.c_str());
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
  };
  rxCharacteristic->setCallbacks(new RxCallbacks());

  service->start();
  advertising = NimBLEDevice::getAdvertising();
  advertising->addServiceUUID(SERVICE_UUID);
  advertising->enableScanResponse(true);
  updateAdvertisementData(1);
  advertising->start();
  Serial.println("Started BLE provisioning");
}

// 如果设备之前已经配网成功，启动后直接连之前保存的 WiFi
void waitSavedWifiConnect() {
  WiFi.disconnect(true);
  delay(100);
  WiFi.mode(WIFI_STA);
  WiFi.begin(config.stassid, config.stapsw);
  while (WiFi.status() != WL_CONNECTED) {
    handleBootCounterStableClear();
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("WiFi connected: ");
  Serial.println(WiFi.localIP());
}

// 上电初始化
void setup() {
  Serial.begin(115200);
  updateBootCounterEarly();
  delay(200);
  pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);

  topic = macSuffixTopic();
  shortCode = makeShortCode();
  loadConfig();

  if (strlen(config.ctopic) == 0) {
    safeCopy(config.ctopic, sizeof(config.ctopic), topic.c_str());
  } else {
    topic = String(config.ctopic);
  }

  if (configFlag) {
    startBleProvisioning();
  } else {
    waitSavedWifiConnect();
    Serial.println("Config success");
  }
}

// BLE 配网状态机
void handleBleProvisioning() {
  if (!configFlag) {
    return;
  }

  if (pendingHelloSeq >= 0 && txSubscribed) {
    sendHello(pendingHelloSeq);
    pendingHelloSeq = -1;
  }

  if (wifiConnectStarted && provisionState == STATE_WIFI_CONNECTING) {
    unsigned long now = millis();

    if (now - wifiLastNotifyMs > 3000) {
      wifiLastNotifyMs = now;
      sendStatus(wifiStatusSeq, 0, "wifi_connecting", 40);
    }

    if (WiFi.status() != WL_CONNECTED &&
        now - wifiLastRetryMs > WIFI_RETRY_INTERVAL_MS) {
      wifiLastRetryMs = now;
      Serial.println("WiFi retry begin");
      WiFi.disconnect();
      delay(100);
      WiFi.begin(config.stassid, config.stapsw);
    }

    if (WiFi.status() == WL_CONNECTED) {
      wifiConnectStarted = false;
      provisionState = STATE_WIFI_CONNECTED;
      sendStatus(wifiStatusSeq, 0, "wifi_connected", 100);
      delay(WIFI_CONNECTED_TO_DONE_GAP_MS);

      if (firstWifiConfig) {
        saveConfig();
        sendStatus(wifiStatusSeq, 0, "done", 100);
        delay(DONE_SETTLE_MS);

        configFlag = false;
        firstWifiConfig = false;
        updateAdvertisementData(0);
        addTopicToBemfa();
      }
    } else if (now - wifiConnectStartMs > WIFI_CONNECT_TIMEOUT_MS) {
      provisionState = STATE_FAILED;
      wifiConnectStarted = false;
      sendStatus(wifiStatusSeq, 1003, "wifi_failed", 100, "wifi_timeout", true);
    }
  }
}

// 每秒打印一次当前 WiFi 状态（调试用）
void handleWifiDebugLog() {
  if (configFlag) {
    return;
  }

  unsigned long now = millis();
  if (now - wifiDebugLastPrintMs < 1000) {
    return;
  }
  wifiDebugLastPrintMs = now;

  String ssid = WiFi.SSID();
  if (ssid.length() == 0) {
    ssid = "(none)";
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi status: connected, SSID: ");
    Serial.println(ssid);
  } else {
    Serial.print("WiFi status: disconnected, SSID: ");
    Serial.println(ssid);
  }
}

// 主循环
void loop() {
  handleBootCounterStableClear();
  handleResetButton();
  handleBleProvisioning();
  handleWifiDebugLog(); // 调试函数，仅开发测试使用，需要注释掉

  // 这里写你的其他业务逻辑。
  delay(20); // 调试函数，仅开发测试使用，需要注释掉
}
```

---

## 14. 代码使用步骤

1. 安装 ESP32 开发板支持包
2. 安装 `NimBLE-Arduino` 和 `ArduinoJson`
3. 新建 Arduino 工程，复制完整代码
4. 根据自己的产品修改 `aptype`、`Name`、`verSion` 等配置
5. 烧录到 ESP32 开发板
6. 打开串口监视器，波特率 `115200`
7. 用巴法 App 搜索 `Bemfa_xxxx` 设备并开始配网

### 重新配网方式

- **长按 BOOT 按键 5 秒**：清空配置并重启，重新进入 BLE 配网模式
- **连续重启 4 次**：自动触发恢复出厂设置

---

## 15. 硬件连接说明

| 功能 | 引脚 | 说明 |
|------|------|------|
| 恢复出厂按键 | GPIO 0 | ESP32 DevKit 自带 BOOT 按钮，长按 5 秒恢复出厂 |
| 蓝牙 | 内置 | ESP32 自带 BLE，无需额外连线 |
| WiFi | 内置 | ESP32 自带 WiFi，无需额外连线 |

> 本示例仅使用 ESP32 开发板内置资源（BLE、WiFi、BOOT 按键），**无需额外硬件连线**即可运行。

---

## 16. 关键时序参数

| 参数 | 值 | 说明 |
|------|-----|------|
| BLE_NOTIFY_GAP_MS | 120ms | BLE Notify 最小发送间隔 |
| WIFI_ACK_TO_STATUS_GAP_MS | 250ms | WiFi ACK 到 status 的间隔 |
| WIFI_CONNECTED_TO_DONE_GAP_MS | 250ms | WiFi 连接到 done 的间隔 |
| DONE_SETTLE_MS | 500ms | done 状态稳定等待时间 |
| WIFI_CONNECT_TIMEOUT_MS | 90000ms | WiFi 连接超时（90秒） |
| WIFI_RETRY_INTERVAL_MS | 5000ms | WiFi 重试间隔 |
| BLE_NOTIFY_RETRY_COUNT | 3 | BLE Notify 重试次数 |
| RESET_HOLD_MS | 5000ms | 恢复出厂长按时间 |
| 稳定运行清零重启计数 | 2000ms | 防止正常重启被误判为连续重启 |