#include "SettingsStore.h"
#include "Touchscreen.h"


SPIClass touchscreenSPI = SPIClass(HSPI);
XPT2046_Touchscreen ts(XPT2046_CS);
bool feature_active = false;
static bool runtimeCalibValid = false;
static uint16_t rtXMin = TOUCH_X_MIN, rtXMax = TOUCH_X_MAX;
static uint16_t rtYMin = TOUCH_Y_MIN, rtYMax = TOUCH_Y_MAX;
static bool rtSwapXY = false;

void setupTouchscreen() {
    touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
    ts.begin(touchscreenSPI);
    ts.setRotation(0);
}

extern XPT2046_Touchscreen ts;

void setRuntimeTouchCalibration(uint16_t xMin, uint16_t xMax,
                                uint16_t yMin, uint16_t yMax,
                                bool swapXY) {
  rtXMin = xMin; rtXMax = xMax;
  rtYMin = yMin; rtYMax = yMax;
  rtSwapXY = swapXY;
  runtimeCalibValid = true;
}

bool readTouchXY(int& x, int& y) {
  if (!ts.touched()) return false;
  TS_Point p = ts.getPoint();

  const uint16_t zThresh = 500;
  if (p.z < zThresh) return false;

  const auto& s = settings();
  uint16_t xMin = runtimeCalibValid ? rtXMin : s.touchXMin;
  uint16_t xMax = runtimeCalibValid ? rtXMax : s.touchXMax;
  uint16_t yMin = runtimeCalibValid ? rtYMin : s.touchYMin;
  uint16_t yMax = runtimeCalibValid ? rtYMax : s.touchYMax;
  bool swapXY = runtimeCalibValid ? rtSwapXY : s.touchSwapXY;

  int rawX = p.x;
  int rawY = p.y;
  if (swapXY) {
    int t = rawX;
    rawX = rawY;
    rawY = t;
  }

  // Guard against corrupted calibration values.
  if (abs((int)xMax - (int)xMin) < 100) { xMin = TOUCH_X_MIN; xMax = TOUCH_X_MAX; }
  if (abs((int)yMax - (int)yMin) < 100) { yMin = TOUCH_Y_MIN; yMax = TOUCH_Y_MAX; }

  x = ::map(rawX, xMin, xMax, 0, TFT_WIDTH - 1);
  y = ::map(rawY, yMin, yMax, 0, TFT_HEIGHT - 1);
  x = constrain(x, 0, TFT_WIDTH - 1);
  y = constrain(y, 0, TFT_HEIGHT - 1);
  return true;
}

bool readTouchXYDismiss(int& x, int& y) {
  if (!ts.touched()) return false;
  TS_Point p = ts.getPoint();

  /* Light taps often stay below normal UI threshold (500). */
  const uint16_t zThresh = 120;
  if (p.z < zThresh) return false;

  const auto& s = settings();
  uint16_t xMin = runtimeCalibValid ? rtXMin : s.touchXMin;
  uint16_t xMax = runtimeCalibValid ? rtXMax : s.touchXMax;
  uint16_t yMin = runtimeCalibValid ? rtYMin : s.touchYMin;
  uint16_t yMax = runtimeCalibValid ? rtYMax : s.touchYMax;
  bool swapXY = runtimeCalibValid ? rtSwapXY : s.touchSwapXY;

  int rawX = p.x;
  int rawY = p.y;
  if (swapXY) {
    int t = rawX;
    rawX = rawY;
    rawY = t;
  }

  if (abs((int)xMax - (int)xMin) < 100) { xMin = TOUCH_X_MIN; xMax = TOUCH_X_MAX; }
  if (abs((int)yMax - (int)yMin) < 100) { yMin = TOUCH_Y_MIN; yMax = TOUCH_Y_MAX; }

  x = ::map(rawX, xMin, xMax, 0, TFT_WIDTH - 1);
  y = ::map(rawY, yMin, yMax, 0, TFT_HEIGHT - 1);
  x = constrain(x, 0, TFT_WIDTH - 1);
  y = constrain(y, 0, TFT_HEIGHT - 1);
  return true;
}
