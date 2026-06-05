#ifndef BEMFA_FRAMEWORK_H
#define BEMFA_FRAMEWORK_H

#include "bemfa_app.h"
#include "led_controller.h"

class BemfaFramework {
public:
  static void begin(const BemfaAppConfig& app);
  static void loop();

  static bool isWifiConnected();
  static bool isMqttConnected();
  static bool isProvisioning();
  static bool publish(const char* topic, const char* payload, bool retained = false);
  static LEDController& led();
};

#endif
