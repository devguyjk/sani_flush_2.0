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
  scrollOffset = 0;  // Initialize scroll offset
  
  // Initialize settings with 8 items to test scrolling
  settings[0] = {"Flush Delay Time", "ms", 500, 0, 5000, 50, false, "delayTime"};
  settings[1] = {"Flush Time Gap", "ms", 1000, 100, 10000, 100, false, "timeGap"};
  settings[2] = {"Waste Qty Per Flush", "oz", 16, 1, 100, 1, false, "wasteQty"};
  settings[3] = {"Flush to Snap Pic", "", 1, 0, 1, 1, false, "snapPic"};
  settings[4] = {"Auto Flush", "", 0, 0, 1, 1, false, "autoFlush"};
  settings[5] = {"Night Mode", "", 1, 0, 1, 1, false, "nightMode"};
  settings[6] = {"Sound Volume", "%", 75, 0, 100, 5, false, "volume"};
  settings[7] = {"Screen Timeout", "sec", 60, 10, 300, 10, false, "timeout"};
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
  Serial.printf("Touch detected: X=%d, Y=%d\n", x, y);
  
  // Back button
  if(x >= 10 && x <= 50 && y >= 10 && y <= 40) {
    Serial.println("Back button touched - hiding settings!");
    resetEditingStates();
    settingsVisible = false;
    scrollOffset = 0;
    return;
  }
  
  // Scroll arrow buttons - LARGER TOUCH AREAS
  if(x >= 200 && x <= 235) {
    Serial.println("Touch in scroll arrow area");
    
    // Up arrow - LARGER touch area
    if(y >= 55 && y <= 90) {
      Serial.println("UP arrow touched!");
      if(scrollOffset > 0) {
        scrollOffset -= 35; // Scroll UP (show previous items)
        if(scrollOffset < 0) scrollOffset = 0;
        Serial.println("Scrolled up - New offset: " + String(scrollOffset));
        drawInterface();
      } else {
        Serial.println("Already at top - cannot scroll up");
      }
      return;
    }
    
    // Down arrow - LARGER touch area
    if(y >= 275 && y <= 320) {  // Extended to bottom of screen
      Serial.println("DOWN arrow touched!");
      int maxScroll = max(0, (8 * 35) - (320 - 55 - 10));
      Serial.println("Max scroll: " + String(maxScroll));
      if(scrollOffset < maxScroll) {
        scrollOffset += 35; // Scroll DOWN (show next items)
        if(scrollOffset > maxScroll) scrollOffset = maxScroll;
        Serial.println("Scrolled down - New offset: " + String(scrollOffset));
        drawInterface();
      } else {
        Serial.println("Already at bottom - cannot scroll down");
      }
      return;
    }
    
    Serial.println("Touch in scroll area but outside button ranges");
    Serial.println("Up: Y=55-90, Down: Y=275-320");
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
      
      Serial.println("Setting " + String(i) + " (" + String(settings[i].label) + ") touched!");
      
      if(settings[i].editing) {
        // Handle value editing
        if(x < 120) {
          settings[i].value = max(settings[i].minVal, 
            settings[i].value - settings[i].step);
          Serial.println("Decreased to: " + String(settings[i].value));
        } else {
          settings[i].value = min(settings[i].maxVal, 
            settings[i].value + settings[i].step);
          Serial.println("Increased to: " + String(settings[i].value));
        }
        saveSettings();
      } else {
        // Enter edit mode
        Serial.println("Entering edit mode for: " + String(settings[i].label));
        resetEditingStates();
        settings[i].editing = true;
      }
      drawInterface();
      return;
    }
  }
  
  Serial.println("Touch not handled - outside all areas");
}

void SettingsSystem::loadSettings() {
  for(int i = 0; i < 8; i++) {
    settings[i].value = prefs.getInt(settings[i].prefKey, settings[i].value);
  }
  Serial.println("Flush settings loaded from memory");
}

void SettingsSystem::saveSettings() {
  for(int i = 0; i < 8; i++) {
    prefs.putInt(settings[i].prefKey, settings[i].value);
  }
  Serial.println("Flush settings saved to memory");
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
  
  // Debug info
  Serial.println("Up arrow: X=200-235, Y=55-90");
  Serial.println("Down arrow: X=200-235, Y=275-310");
  Serial.println("Current scroll offset: " + String(scrollOffset));
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
    tft->drawString("◀", x + w - 35, y + 18);
    tft->drawString("▶", x + w - 10, y + 18);
    
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
  if(setting.maxVal > setting.minVal && String(setting.label) != "Flush to Snap Pic") {
    String range = String(setting.minVal) + "-" + String(setting.maxVal);
    tft->setTextColor(SETTINGS_TEXT_SEC_COLOR);
    tft->setTextSize(1);
    tft->setTextDatum(TL_DATUM);
    tft->drawString(range, x + 6, y + 18);
  }
}

String SettingsSystem::formatSettingValue(FlushSetting setting) {
  String result;
  
  if(String(setting.label) == "Flush to Snap Pic") {
    result = setting.value ? "ON" : "OFF";
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
int SettingsSystem::getFlushDelayTime() {
  return settings[0].value;
}

int SettingsSystem::getFlushTimeGap() {
  return settings[1].value;
}

int SettingsSystem::getWasteQtyPerFlush() {
  return settings[2].value;
}

bool SettingsSystem::getFlushToSnapPic() {
  return settings[3].value == 1;
}