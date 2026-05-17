#include <ArduinoJson.h>
#include <SD.h>
#include <Preferences.h>
#include "SettingsStore.h"


static AppSettings g_settings;
AppSettings& settings() { return g_settings; }
static Preferences g_prefs;
static bool touchCalibLooksValid(uint16_t xMin, uint16_t xMax, uint16_t yMin, uint16_t yMax) {
  return (abs((int)xMax - (int)xMin) >= 100) &&
         (abs((int)yMax - (int)yMin) >= 100);
}

static bool loadTouchFromNVS() {
  if (!g_prefs.begin("touchcal", true)) return false;
  auto& s = g_settings;
  bool has = g_prefs.getBool("has", false);
  if (has) {
    uint16_t xMin = g_prefs.getUShort("xMin", s.touchXMin);
    uint16_t xMax = g_prefs.getUShort("xMax", s.touchXMax);
    uint16_t yMin = g_prefs.getUShort("yMin", s.touchYMin);
    uint16_t yMax = g_prefs.getUShort("yMax", s.touchYMax);
    bool swapXY   = g_prefs.getBool("swap", s.touchSwapXY);
    if (touchCalibLooksValid(xMin, xMax, yMin, yMax)) {
      s.touchXMin = xMin;
      s.touchXMax = xMax;
      s.touchYMin = yMin;
      s.touchYMax = yMax;
      s.touchSwapXY = swapXY;
    } else {
      has = false;
    }
  }
  g_prefs.end();
  return has;
}

static bool saveTouchToNVS() {
  if (!g_prefs.begin("touchcal", false)) return false;
  const auto& s = g_settings;
  bool ok = true;
  ok &= g_prefs.putBool("has", true) > 0;
  ok &= g_prefs.putUShort("xMin", s.touchXMin) > 0;
  ok &= g_prefs.putUShort("xMax", s.touchXMax) > 0;
  ok &= g_prefs.putUShort("yMin", s.touchYMin) > 0;
  ok &= g_prefs.putUShort("yMax", s.touchYMax) > 0;
  ok &= g_prefs.putBool("swap", s.touchSwapXY) > 0;
  g_prefs.end();
  return ok;
}

static bool sd_mounted = false;
static bool mountSD() {

  if (sd_mounted) {
    if (SD.exists("/")) return true;
    sd_mounted = false;
  }
  #ifdef SD_CS
  if (SD.begin(SD_CS)) { sd_mounted = true; return true; }
  #endif
  #ifdef SD_CS_PIN

  #ifdef CC1101_CS
  if (SD_CS_PIN != CC1101_CS) {
    if (SD.begin(SD_CS_PIN)) { sd_mounted = true; return true; }
  }
  #else
  if (SD.begin(SD_CS_PIN)) { sd_mounted = true; return true; }
  #endif
  #endif
  return false;
}

static bool ensureDir(const char* dirPath) {
  if (!mountSD()) return false;
  if (!SD.exists(dirPath)) {
    if (SD.mkdir(dirPath)) return true;

    if (dirPath && dirPath[0] == '/') {
      return SD.mkdir(dirPath + 1);
    }
    return false;
  }
  return true;
}

bool settingsLoad() {
  if (!mountSD()) {
    loadTouchFromNVS();
    return false;
  }
  if (!SD.exists(SETTINGS_PATH)) {
    loadTouchFromNVS();
    return true;
  }

  File f = SD.open(SETTINGS_PATH, FILE_READ);
  if (!f) return false;

  StaticJsonDocument<512> doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) return false;

  auto& s = g_settings;
  s.brightness      = doc["brightness"]      | s.brightness;
  s.theme           = (Theme)(uint8_t)(doc["theme"] | (uint8_t)s.theme);
  s.neopixelEnabled = doc["neopixelEnabled"] | s.neopixelEnabled;

  s.autoWifiScan    = doc["autoWifiScan"]    | s.autoWifiScan;
  s.autoBleScan     = doc["autoBleScan"]     | s.autoBleScan;

  if (s.autoWifiScan != s.autoBleScan) {
    bool en = (s.autoWifiScan || s.autoBleScan);
    s.autoWifiScan = en;
    s.autoBleScan  = en;
  }

  s.touchXMin       = doc["touch"]["xMin"]   | s.touchXMin;
  s.touchXMax       = doc["touch"]["xMax"]   | s.touchXMax;
  s.touchYMin       = doc["touch"]["yMin"]   | s.touchYMin;
  s.touchYMax       = doc["touch"]["yMax"]   | s.touchYMax;
  s.touchSwapXY     = doc["touch"]["swapXY"] | s.touchSwapXY;

  return true;
}

bool settingsSave() {

  if (!ensureDir("/config")) {
    sd_mounted = false;
    if (!ensureDir("/config")) {
      return saveTouchToNVS();
    }
  }

  File f = SD.open(SETTINGS_PATH, FILE_WRITE);
  if (!f) {
    sd_mounted = false;
    if (!mountSD()) return saveTouchToNVS();
    f = SD.open(SETTINGS_PATH, FILE_WRITE);
    if (!f) return saveTouchToNVS();
  }

  auto& s = g_settings;
  StaticJsonDocument<512> doc;
  doc["brightness"]      = s.brightness;
  doc["theme"]           = (uint8_t)s.theme;
  doc["neopixelEnabled"] = s.neopixelEnabled;

  doc["autoWifiScan"]    = s.autoWifiScan;
  doc["autoBleScan"]     = s.autoBleScan;

  JsonObject t = doc.createNestedObject("touch");
  t["xMin"] = s.touchXMin;
  t["xMax"] = s.touchXMax;
  t["yMin"] = s.touchYMin;
  t["yMax"] = s.touchYMax;
  t["swapXY"] = s.touchSwapXY;

  bool ok = serializeJson(doc, f) > 0;
  f.close();
  bool nvsOk = saveTouchToNVS();
  return ok || nvsOk;
}

bool settingsSaveTouchOnly() {
  return saveTouchToNVS();
}
