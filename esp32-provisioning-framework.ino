/**
 * ESP32 巴法云配网 + MQTT 框架 — 入口文件
 *
 * 业务逻辑请修改 src/app/ 目录，本文件一般无需改动。
 *
 * LED 状态指示：
 * - 蓝色持续闪烁：蓝牙配网模式
 * - 绿色快速闪 3 次：配网成功
 * - 蓝色快速闪 3 次：MQTT 已连接
 */

#include "framework/bemfa_framework.h"
#include "app/app_handlers.h"

void setup() {
  Serial.begin(115200);
  delay(1000);  // USB CDC 就绪需要时间
  BemfaFramework::begin(getAppConfig());
}

void loop() {
  BemfaFramework::loop();
}
