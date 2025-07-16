#include "settings_system.h"

// Customizable color variables - modify these to match your design
uint16_t SETTINGS_BG_COLOR = TFT_WHITE;        // White background
uint16_t SETTINGS_CARD_COLOR = 0xF7BE;         // Light gray cards
uint16_t SETTINGS_PRIMARY_COLOR = 0x001F;      // Dark blue (logo color)
uint16_t SETTINGS_TEXT_COLOR = TFT_BLACK;      // Black text
uint16_t SETTINGS_TEXT_SEC_COLOR = 0x4208;     // Dark gray text
uint16_t SETTINGS_BORDER_COLOR = 0x8410;       // Medium gray borders
uint16_t SETTINGS_ACCENT_COLOR = 0x035F;       // Blue accent

SettingsSystem::SettingsSystem(TFT_eSPI* display) {
  tft = display;
  settingsVisible = false;
  touching = false;
  lastTouch = 0;
  
  // Initialize settings with specific values as per requirements
  settings[0] = {"Right Toilet Delay", "sec", 10, 5, 40, 5, false, "rightDelay"};
  settings[1] = {"Flush Time Lapse", "min", 5, 1, 45, 5, false, "flushTime"};
  settings[2] = {"Waste Trigger Delay", "ms", 5000, 2500, 20000, 2500, false, "wasteDelay"};
  settings[3] = {"Camera Trigger Delay", "ms", 2500, 2500, 20000, 2500, false, "cameraDelay"};
  settings[4] = {"Pump Waste Dose", "ml", 100, 50, 300, 50, false, "pumpDose"};
  settings[5] = {"Toilet Relay Hold", "ms", 2000, 1000, 6000, 1000, false, "relayHold"};
  settings[6] = {"Camera Capture Count", "", 3, 1, 30, 1, false, "cameraCount"};
}

void SettingsSystem::begin() {
  prefs.begin("flush_settings", false);
  loadSettings();
}

void SettingsSystem::showSettings() {
  settingsVisible = true;
  drawInterface();
}

void SettingsSystem::hideSettings() {
  settingsVisible = false;
  resetEditingStates();
}

bool SettingsSystem::isSettingsVisible() {
  return settingsVisible;
}

void SettingsSystem::handleTouch() {
  uint16_t x, y;
  bool pressed = tft->getTouch(&x, &y);
  
  if(pressed && !touching && (millis() - lastTouch > 200)) {
    touching = true;
    lastTouch = millis();
    
    if(settingsVisible) {
      handleSettingsPageTouch(x, y);
    }
  } else if(!pressed) {
    touching = false;
  }
}

void SettingsSystem::handleSettingsPageTouch(int x, int y) {
  // Back button - fix the coordinates and add debug
  if(x >= 10 && x <= 50 && y >= 10 && y <= 40) {
    Serial.println("Back button touched - hiding settings!");
    resetEditingStates();
    settingsVisible = false;  // Make sure this is set
    return;
  }
  
  // Settings items
  int itemHeight = 60;
  int startY = 70;
  
  for(int i = 0; i < 7; i++) {
    int itemY = startY + i * itemHeight;
    
    if(y >= itemY && y <= itemY + 55) {
      if(settings[i].editing) {
        // Handle value editing
        if(x < 120) {
          // Decrease value
          settings[i].value = max(settings[i].minVal, 
            settings[i].value - settings[i].step);
        } else {
          // Increase value
          settings[i].value = min(settings[i].maxVal, 
            settings[i].value + settings[i].step);
        }
        saveSettings(); // Auto-save when value changes
      } else {
        // Enter edit mode
        resetEditingStates();
        settings[i].editing = true;
      }
      drawInterface();
      break;
    }
  }
}

void SettingsSystem::loadSettings() {
  for(int i = 0; i < 7; i++) {
    settings[i].value = prefs.getInt(settings[i].prefKey, settings[i].value);
  }
  Serial.println("Flush settings loaded from memory");
}

void SettingsSystem::saveSettings() {
  for(int i = 0; i < 7; i++) {
    prefs.putInt(settings[i].prefKey, settings[i].value);
  }
  Serial.println("Flush settings saved to memory");
}

void SettingsSystem::drawInterface() {
  tft->fillScreen(SETTINGS_BG_COLOR);
  drawHeader();
  
  // Settings items
  int itemHeight = 60;
  int startY = 70;
  
  for(int i = 0; i < 7; i++) {
    drawSettingItem(i, startY + i * itemHeight);
  }
}

void SettingsSystem::drawHeader() {
  // Header background
  tft->fillRect(0, 0, 240, 50, SETTINGS_PRIMARY_COLOR);
  
  // Back button
  drawBackButton();
  
  // Title
  tft->setTextColor(TFT_WHITE);
  tft->setTextSize(2);
  tft->setTextDatum(MC_DATUM);
  tft->drawString("Flush Settings", 130, 25);
}

void SettingsSystem::drawBackButton() {
  // Back button background
  tft->fillRoundRect(10, 12, 30, 26, 6, TFT_WHITE);
  
  // Back arrow
  tft->setTextColor(SETTINGS_PRIMARY_COLOR);
  tft->setTextSize(2);
  tft->setTextDatum(MC_DATUM);
  tft->drawString("<", 25, 25);
}

void SettingsSystem::drawSettingItem(int index, int y) {
  FlushSetting& setting = settings[index];
  int x = 10;
  int w = 220;
  int h = 55;
  
  // Background
  uint16_t bgColor = setting.editing ? SETTINGS_ACCENT_COLOR : SETTINGS_CARD_COLOR;
  uint16_t textColor = setting.editing ? TFT_WHITE : SETTINGS_TEXT_COLOR;
  
  tft->fillRoundRect(x, y, w, h, 10, bgColor);
  tft->drawRoundRect(x, y, w, h, 10, SETTINGS_BORDER_COLOR);
  
  // Label
  tft->setTextColor(textColor);
  tft->setTextSize(1);
  tft->setTextDatum(TL_DATUM);
  tft->drawString(setting.label, x + 12, y + 10);
  
  // Value display
  String valueStr = formatSettingValue(setting);
  
  if(setting.editing) {
    // Edit mode - show arrows and highlighted value
    tft->setTextColor(TFT_WHITE);
    tft->setTextSize(1);
    tft->setTextDatum(TR_DATUM);
    tft->drawString("◀", x + w - 60, y + 30);
    tft->drawString("▶", x + w - 20, y + 30);
    
    tft->setTextColor(TFT_WHITE);
    tft->setTextSize(2);
    tft->setTextDatum(TC_DATUM);
    tft->drawString(valueStr, x + w - 40, y + 25);
  } else {
    // Normal mode
    tft->setTextColor(SETTINGS_TEXT_SEC_COLOR);
    tft->setTextSize(1);
    tft->setTextDatum(TR_DATUM);
    tft->drawString(valueStr, x + w - 12, y + 30);
  }
}

String SettingsSystem::formatSettingValue(FlushSetting setting) {
  return String(setting.value) + setting.unit;
}

void SettingsSystem::resetEditingStates() {
  for(int i = 0; i < 7; i++) {
    settings[i].editing = false;
  }
}

// Getter functions for main program to access settings
int SettingsSystem::getRightToiletFlushDelaySec() {
  return settings[0].value;
}

int SettingsSystem::getFlushTotalTimeLapseMin() {
  return settings[1].value;
}

int SettingsSystem::getWasteRepoTriggerDelayMs() {
  return settings[2].value;
}

int SettingsSystem::getCameraTriggerAfterFlushMs() {
  return settings[3].value;
}

int SettingsSystem::getPumpWasteDoseML() {
  return settings[4].value;
}

int SettingsSystem::getToiletFlushRelayHoldTimeMS() {
  return settings[5].value;
}

int SettingsSystem::getFlushCountForCameraCapture() {
  return settings[6].value;
}