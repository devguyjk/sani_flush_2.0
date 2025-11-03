#include "settings_system.h"
#include "draw_functions.h" // For writeLog

// Forward declaration for drawFlowDetails
void drawFlowDetails();
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
  scrollOffset = 0;  // Initialize scroll offset
  
  // Initialize settings with 8 items to test scrolling
  settings[0] = {"Flush Relay Time Lapse", "ms", 3000, 1000, 10000, 500, false, "flushRelayTimeLapse"};
  settings[1] = {"Flush Workflow Repeat", "sec", 120, 60, 600, 30, false, "flushWorkflowRepeat"};
  settings[2] = {"Waste Qty Per Flush", "ml", 50, 25, 3000, 25, false, "wasteQty"};
  settings[3] = {"Pic Every N Flushes", "", 2, 1, 10, 1, false, "picFlushes"};
  settings[4] = {"Waste Repo Pump Delay", "sec", 7, 1, 20, 1, false, "wasteDelay"};
  settings[5] = {"Camera Pic Delay", "ms", 25000, 500, 30000, 500, false, "cameraPicDelay"};
  settings[6] = {"Flush right, after left flush delay", "sec", 10, 1, 100, 1, false, "flushRighDelay"};
  settings[7] = {"Screen Timeout", "sec", 60, 10, 300, 10, false, "timeout"};
  settings[8] = {"Volume of water per flush left toilet", "oz", 128, 60, 1000, 12, false, "leftTWaterFlushed"};
  settings[9] = {"Volume of water per flush right toilet", "oz", 128, 60, 1000, 12, false, "rightTWaterFlushed"};
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
  writeLog("Settings Touch: X=%d, Y=%d", x, y);
  
  // Back button
  if(x >= 10 && x <= 50 && y >= 10 && y <= 40) {
    writeLog("Settings: Back button touched");
    resetEditingStates();
    settingsVisible = false;
    scrollOffset = 0;
    return;
  }
  
  // Scroll arrow buttons - LARGER TOUCH AREAS
  if(x >= 200 && x <= 235) {
    // writeLog("Touch in scroll arrow area"); // Too verbose
    
    // Up arrow - LARGER touch area
    if(y >= 55 && y <= 90) {
      writeLog("Settings: Scroll UP touched");
      if(scrollOffset > 0) {
        scrollOffset -= 35; // Scroll UP (show previous items)
        if(scrollOffset < 0) scrollOffset = 0;
        writeLog("Settings: Scrolled up - New offset: %d", scrollOffset);
        drawInterface();
      } else {
        writeLog("Settings: Already at top - cannot scroll up");
      }
      return;
    }
    
    // Down arrow - LARGER touch area
    if(y >= 275 && y <= 320) {  // Extended to bottom of screen
      writeLog("Settings: Scroll DOWN touched");
      int maxScroll = max(0, (8 * 35) - (320 - 55 - 10));
      // writeLog("Max scroll: %d", maxScroll); // Too verbose
      if(scrollOffset < maxScroll) {
        scrollOffset += 35; // Scroll DOWN (show next items)
        if(scrollOffset > maxScroll) scrollOffset = maxScroll;
        writeLog("Settings: Scrolled down - New offset: %d", scrollOffset);
        drawInterface();
      } else {
        writeLog("Settings: Already at bottom - cannot scroll down");
      }
      return;
    }
  }
  
  // Settings items - updated for compact layout
  int itemHeight = 35;
  int startY = 55;
  
  for(int i = 0; i < 8; i++) {
    int itemY = startY + (i * itemHeight) - scrollOffset;
    
    // Check if item is visible and touched (exclude right scroll area)
    if(y >= itemY && y <= itemY + 30 && 
       x >= 10 && x <= 195 &&  // Reduced to avoid scroll arrows
       itemY >= startY && itemY <= (320 - 10 - itemHeight)) {
      
      writeLog("Settings: Item %d (%s) touched", i, settings[i].label);
      
      if(settings[i].editing) {
        // Handle value editing
        if(x < 120) {
          settings[i].value = max(settings[i].minVal, 
            settings[i].value - settings[i].step);
          writeLog("Settings: Decreased '%s' to %d", settings[i].label, settings[i].value);
        } else {
          settings[i].value = min(settings[i].maxVal, 
            settings[i].value + settings[i].step);
          writeLog("Settings: Increased '%s' to %d", settings[i].label, settings[i].value);
        }
        saveSettings();
      } else {
        // Enter edit mode
        writeLog("Settings: Entering edit mode for '%s'", settings[i].label);
        resetEditingStates();
        settings[i].editing = true;
      }
      drawInterface();
      return;
    }
  }
}

void SettingsSystem::loadSettings() {
  // FORCE DEFAULTS: Use code defaults instead of flash memory
  writeLog("Using code defaults (ignoring flash memory)");
  for(int i = 0; i < 8; i++) {
    writeLog("Setting %d (%s): %d", i, settings[i].label, settings[i].value);
    writeLog("Settings: Loading default '%s' = %d", settings[i].label, settings[i].value);
  }
  writeLog("All settings loaded from code defaults");
}

void SettingsSystem::saveSettings() {
  for(int i = 0; i < 8; i++) {
    prefs.putInt(settings[i].prefKey, settings[i].value);
  }
  writeLog("Flush settings saved to memory");
  
  // Update flow details when settings change
  if (!settingsVisible) {
    drawFlowDetails();
  }
}

void SettingsSystem::drawInterface() {
  tft->fillScreen(SETTINGS_BG_COLOR);
  drawHeader();
  
  // Compact scrollable settings
  int itemHeight = 35;
  int startY = 55;
  int availableHeight = 320 - startY - 10;
  
  // Draw visible items based on scroll offset
  for(int i = 0; i < 8; i++) {
    int itemY = startY + (i * itemHeight) - scrollOffset;
    
    // Only draw items that are visible on screen
    if(itemY >= startY && itemY <= (startY + availableHeight - itemHeight)) {
      drawSettingItem(i, itemY);
    }
  }
  
  // Up arrow - TOP RIGHT (just below header) - LARGER TOUCH AREA
  tft->fillRoundRect(200, 55, 35, 35, 6, SETTINGS_PRIMARY_COLOR);
  tft->setTextColor(TFT_WHITE);
  tft->setTextSize(3);
  tft->setTextDatum(MC_DATUM);
  tft->drawString("^", 217, 72);  // Draw simple ^ character
  
  // Down arrow - BOTTOM RIGHT - LARGER TOUCH AREA
  tft->fillRoundRect(200, 275, 35, 35, 6, SETTINGS_PRIMARY_COLOR);
  tft->setTextColor(TFT_WHITE);
  tft->setTextSize(3);
  tft->setTextDatum(MC_DATUM);
  // Draw inverted triangle using multiple lines
  tft->drawString("v", 217, 292);  // Draw simple v character
  // writeLog("Current scroll offset: %d", scrollOffset); // Too verbose
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
  tft->drawString("Flush Settings", 120, 25);
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
  int w = 178;  // Reduced width to make room for scroll arrows
  int h = 30;   // Reduced from 40 to 30
  
  // Background
  uint16_t bgColor = setting.editing ? SETTINGS_ACCENT_COLOR : SETTINGS_CARD_COLOR;
  uint16_t textColor = setting.editing ? TFT_WHITE : SETTINGS_TEXT_COLOR;
  
  tft->fillRoundRect(x, y, w, h, 6, bgColor);
  tft->drawRoundRect(x, y, w, h, 6, SETTINGS_BORDER_COLOR);
  
  // Label - more compact
  tft->setTextColor(textColor);
  tft->setTextSize(1);
  tft->setTextDatum(TL_DATUM);
  tft->drawString(setting.label, x + 6, y + 4);
  
  // Value display - more compact
  String valueStr = formatSettingValue(setting);
  
  if(setting.editing) {
    // Edit mode - show arrows and highlighted value
    tft->setTextColor(TFT_WHITE);
    tft->setTextSize(1);
    tft->setTextDatum(TR_DATUM);
    tft->drawString("<", x + w - 35, y + 18);
    tft->drawString(">", x + w - 10, y + 18);
    
    tft->setTextColor(TFT_WHITE);
    tft->setTextSize(1);
    tft->setTextDatum(TC_DATUM);
    tft->drawString(valueStr, x + w - 22, y + 18);
  } else {
    // Normal mode
    tft->setTextColor(SETTINGS_TEXT_SEC_COLOR);
    tft->setTextSize(1);
    tft->setTextDatum(TR_DATUM);
    tft->drawString(valueStr, x + w - 6, y + 18);
  }
  
  // Very compact range info (if applicable)
  if(setting.maxVal > setting.minVal && String(setting.label) != "Pic Every N Flushes") {
    String range = String(setting.minVal) + "-" + String(setting.maxVal);
    tft->setTextColor(SETTINGS_TEXT_SEC_COLOR);
    tft->setTextSize(1);
    tft->setTextDatum(TL_DATUM);
    tft->drawString(range, x + 6, y + 18);
  }
}

String SettingsSystem::formatSettingValue(FlushSetting setting) {
  String result;
  
  if(String(setting.label) == "Pic Every N Flushes") {
    result = String(setting.value);
  } else {
    result = String(setting.value) + setting.unit;
  }
  
  return result;
}

void SettingsSystem::resetEditingStates() {
  for(int i = 0; i < 8; i++) {
    settings[i].editing = false;
  }
}

// Getter functions for main program to access settings
int SettingsSystem::getFlushRelayTimeLapse() {
  return settings[0].value;
}

int SettingsSystem::getFlushWorkflowRepeat() {
  return settings[1].value * 1000; // Convert seconds to milliseconds
}

int SettingsSystem::getWasteQtyPerFlush() {
  return settings[2].value;
}

int SettingsSystem::getPicEveryNFlushes() {
  return settings[3].value;
}

int SettingsSystem::getRightToiletFlushDelaySec() {
  return settings[6].value; // "Flush right, after left flush delay" in seconds
}



int SettingsSystem::getWasteRepoTriggerDelayMs() {
  int result = settings[4].value * 1000; // Convert seconds to milliseconds
  return result;
}

int SettingsSystem::getCameraTriggerAfterFlushMs() {
  return settings[5].value; // Already in milliseconds
}

int SettingsSystem::getPumpWasteDoseML() {
  return 50; // Default 50ml
}

int SettingsSystem::getToiletFlushRelayHoldTimeMS() {
  return settings[0].value; // Use setting value
}

int SettingsSystem::getFlushCountForCameraCapture() {
  return settings[3].value; // Use setting value
}

int SettingsSystem::getLeftToiletWaterOz() {
  return settings[8].value; // Left toilet water volume in oz
}

int SettingsSystem::getRightToiletWaterOz() {
  return settings[9].value; // Right toilet water volume in oz
}

