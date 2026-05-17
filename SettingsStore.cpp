#include <ArduinoJson.h>
#include <SD.h>
#include "SettingsStore.h"
#include "utils.h"


static AppSettings g_settings;
AppSettings& settings() { return g_settings; }
static Preferences g_prefs;
static uint8_t normalizeSleepSeconds(uint8_t v) {
  if (v == 15 || v == 30 || v == 60) return v;
  return 0;
}
static bool touchCalibLooksValid(uint16_t xMin, uint16_t xMax, uint16_t yMin, uint16_t yMax) {
  return (abs((int)xMax - (int)xMin) >= 100) &&
         (abs((int)yMax - (int)yMin) >= 100);
}

uint16_t accentColor565(uint8_t preset) {
  return kAccentPresets[accentPresetClamp(preset)].color565;
}

const char* accentPresetName(uint8_t preset) {
  return kAccentPresets[accentPresetClamp(preset)].name;
}

const char* settingsBoardProfileId() {
  return TOUCH_PROFILE_ID;
}

void settingsApplyBoardTouchDefaults() {
  auto& s = g_settings;
  s.touchXMin = TOUCH_X_MIN;
  s.touchXMax = TOUCH_X_MAX;
  s.touchYMin = TOUCH_Y_MIN;
  s.touchYMax = TOUCH_Y_MAX;
}

static bool settingsTouchSavedForBoard(const StaticJsonDocument<512>& doc) {
  JsonObjectConst touch = doc["touch"];
  if (touch.isNull()) {
    return false;
  }
  if (!touch["xMin"].is<uint16_t>() || !touch["xMax"].is<uint16_t>() ||
      !touch["yMin"].is<uint16_t>() || !touch["yMax"].is<uint16_t>()) {
    return false;
  }
  const char* savedBoard = doc["board"] | "";
  if (savedBoard[0] == '\0') {
    return true;
  }
  return strcmp(savedBoard, TOUCH_PROFILE_ID) == 0;
}

static bool sd_mounted = false;
static bool mountSD() {

  if (sd_mounted) {
    if (SD.exists("/")) return true;
    sd_mounted = false;
  }

  sdSpiInit();

  #ifdef SD_CS
  if (sdMountChipSelect(SD_CS)) { sd_mounted = true; return true; }
  #endif
  #ifdef SD_CS_PIN

  #ifdef CC1101_CS
  if (SD_CS_PIN != CC1101_CS) {
    if (sdMountChipSelect(SD_CS_PIN)) { sd_mounted = true; return true; }
  }
  #else
  if (sdMountChipSelect(SD_CS_PIN)) { sd_mounted = true; return true; }
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
  settingsApplyBoardTouchDefaults();
  if (!mountSD()) return false;
  if (!SD.exists(SETTINGS_PATH)) return true;

  File f = SD.open(SETTINGS_PATH, FILE_READ);
  if (!f) return false;

  StaticJsonDocument<512> doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) return false;

  auto& s = g_settings;
  s.brightness      = doc["brightness"]      | s.brightness;
  s.theme           = (Theme)(uint8_t)(doc["theme"] | (uint8_t)s.theme);
  s.accentColor     = accentPresetClamp(doc["accentColor"] | s.accentColor);
  s.neopixelEnabled = doc["neopixelEnabled"] | s.neopixelEnabled;
  uint8_t sleepVal = (uint8_t)(doc["sleepSeconds"] | s.sleepSeconds);
  if (!doc.containsKey("sleepSeconds")) {
    // Backward compatibility with previous key name.
    sleepVal = (uint8_t)(doc["sleepMinutes"] | sleepVal);
  }
  s.sleepSeconds    = normalizeSleepSeconds(sleepVal);

  s.autoWifiScan    = doc["autoWifiScan"]    | s.autoWifiScan;
  s.autoBleScan     = doc["autoBleScan"]     | s.autoBleScan;

  if (s.autoWifiScan != s.autoBleScan) {
    bool en = (s.autoWifiScan || s.autoBleScan);
    s.autoWifiScan = en;
    s.autoBleScan  = en;
  }

  if (settingsTouchSavedForBoard(doc)) {
    JsonObjectConst touch = doc["touch"];
    s.touchXMin = touch["xMin"] | s.touchXMin;
    s.touchXMax = touch["xMax"] | s.touchXMax;
    s.touchYMin = touch["yMin"] | s.touchYMin;
    s.touchYMax = touch["yMax"] | s.touchYMax;
  } else {
    settingsApplyBoardTouchDefaults();
  }

  return true;
}

bool settingsSave() {

  if (!ensureDir("/config")) {
    sd_mounted = false;
    if (!ensureDir("/config")) return false;
  }

  File f = SD.open(SETTINGS_PATH, FILE_WRITE);
  if (!f) {
    sd_mounted = false;
    if (!mountSD()) return false;
    f = SD.open(SETTINGS_PATH, FILE_WRITE);
    if (!f) return false;
  }

  auto& s = g_settings;
  StaticJsonDocument<512> doc;
  doc["board"]           = TOUCH_PROFILE_ID;
  doc["brightness"]      = s.brightness;
  doc["theme"]           = (uint8_t)s.theme;
  doc["accentColor"]     = s.accentColor;
  doc["neopixelEnabled"] = s.neopixelEnabled;
  doc["sleepSeconds"]    = normalizeSleepSeconds(s.sleepSeconds);

  doc["autoWifiScan"]    = s.autoWifiScan;
  doc["autoBleScan"]     = s.autoBleScan;

  JsonObject t = doc.createNestedObject("touch");
  t["xMin"] = s.touchXMin;
  t["xMax"] = s.touchXMax;
  t["yMin"] = s.touchYMin;
  t["yMax"] = s.touchYMax;

  bool ok = serializeJson(doc, f) > 0;
  f.close();
  return ok;
}
