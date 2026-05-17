#pragma once

#include <Arduino.h>
#include "shared.h"

#ifndef SETTINGS_PATH
#define SETTINGS_PATH "/config/settings.json"
#endif

struct AppSettings {

  uint8_t  brightness = BKL_LEVEL_MED;
  Theme    theme      = Theme::Dark;

  bool     neopixelEnabled = false;
  uint8_t  sleepSeconds    = 0;   // 0=Off, 15/30/60 supported

  bool     autoWifiScan    = true;
  bool     autoBleScan     = true;

  uint16_t touchXMin = TOUCH_X_MIN;
  uint16_t touchXMax = TOUCH_X_MAX;
  uint16_t touchYMin = TOUCH_Y_MIN;
  uint16_t touchYMax = TOUCH_Y_MAX;
  bool     touchSwapXY = false;

};

AppSettings& settings();

bool settingsLoad();
bool settingsSave();
bool settingsSaveTouchOnly();
