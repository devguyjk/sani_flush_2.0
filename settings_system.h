#ifndef SETTINGS_SYSTEM_H
#define SETTINGS_SYSTEM_H

#include <TFT_eSPI.h>
#include <Preferences.h>

// Color scheme for white background - customizable variables
extern uint16_t SETTINGS_BG_COLOR;      // White background
extern uint16_t SETTINGS_CARD_COLOR;    // Light gray cards
extern uint16_t SETTINGS_PRIMARY_COLOR; // Dark blue (logo color)
extern uint16_t SETTINGS_TEXT_COLOR;    // Black text
extern uint16_t SETTINGS_TEXT_SEC_COLOR; // Dark gray text
extern uint16_t SETTINGS_BORDER_COLOR;  // Medium gray borders
extern uint16_t SETTINGS_ACCENT_COLOR;  // Blue accent

struct FlushSetting {
  const char* label;
  const char* unit;
  int value;
  int minVal;
  int maxVal;
  int step;
  bool editing;
  const char* prefKey;
};

class SettingsSystem {
public:
  SettingsSystem(TFT_eSPI* display);
  void begin();
  void handleTouch();
  void showSettings();
  void hideSettings();
  bool isSettingsVisible();
  
  // Get setting values for main program to use
  int getFlushRelayTimeLapse();
  int getFlushWorkflowRepeat();
  int getWasteQtyPerFlush();
  int getPicEveryNFlushes();
  
  // Additional getters for compatibility
  int getRightToiletFlushDelaySec();

  int getWasteRepoTriggerDelayMs();
  int getCameraTriggerAfterFlushMs();
  int getPumpWasteDoseML();
  int getToiletFlushRelayHoldTimeMS();
  int getFlushCountForCameraCapture();
  
private:
  TFT_eSPI* tft;
  Preferences prefs;
  FlushSetting settings[8];  // Increased from 4 to 8
  bool settingsVisible;
  bool touching;
  unsigned long lastTouch;
  int scrollOffset;  // Add scroll offset
  
  void loadSettings();
  void saveSettings();
  void drawInterface();
  void drawHeader();
  void drawBackButton();
  void drawSettingItem(int index, int y);
  String formatSettingValue(FlushSetting setting);
  void handleSettingsPageTouch(int x, int y);
  void resetEditingStates();
};

#endif