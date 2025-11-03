#ifndef GLOBAL_VARS_H
#define GLOBAL_VARS_H

#include <TFT_eSPI.h>
#include <LiquidCrystal_I2C.h>
#include <HTTPClient.h>

// Forward declaration of Shape class
class Shape;

// Global Constants (ALL_CAPS_WITH_UNDERSCORES)
extern const int DEFAULT_PADDING;
extern const int CAMERA_HEIGHT;
extern const int CAMERA_WIDTH;
extern const int TOILET_HEIGHT;
extern const int TOILET_WIDTH;
extern const int SCREEN_WIDTH;
extern const int SCREEN_HEIGHT;
extern const int LOGO_WIDTH;
extern const int LOGO_HEIGHT;
extern const int WASTE_REPO_HEIGHT;
extern const int WASTE_REPO_WIDTH;
extern const uint16_t BTN_TRIANGLE_COLOR;
extern const uint16_t BTN_SQUARE_COLOR;

extern int RIGHT_TOILET_FLUSH_DELAY_MS;

// Camera and server configuration
extern const char* left01CameraID;
extern const char* left02CameraID;
extern const char* right01CameraID;
extern const char* right02CameraID;
extern const char* uploadServerURL;

// Hardware pin definitions
extern const int RELAY_P1_PIN;
extern const int RELAY_P2_PIN;
extern const int RELAY_T1_PIN;
extern const int RELAY_T2_PIN;

// UI constants
extern const unsigned long BUTTON_DEBOUNCE_MS;

// Animation timing constants
extern const int TOILET_ANIM_STAGE_DURATION_MS;
extern const int CAMERA_FLASH_STAGE_DURATION_MS;
extern const int WASTE_REPO_ANIM_STAGE_DURATION_MS;
extern const int TOILET_ANIM_TOTAL_STAGES;
extern const int CAMERA_FLASH_TOTAL_STAGES;
extern const int WASTE_REPO_ANIM_TOTAL_STAGES;

// Relay timing constants
extern const int PUMP_WASTE_ML_SEC;
extern int TOILET_FLUSH_HOLD_TIME_MS;

// Flush flow variables (configurable) - now use settings
extern int _flushCountForCameraCapture;

// GLOBAL VARIABLES (prefixed with _)
extern bool _flushLeft;
extern bool _flushRight;
extern bool _flashCameraLeft;
extern bool _flashCameraRight;
extern bool _animateWasteRepoLeft;
extern bool _debugPrintShapeDetails;
extern bool _animateWasteRepoRight;
extern bool _drawTriangle;
extern int _timerLeftMinutes;
extern int _timerLeftSeconds;
extern bool _timerLeftRunning;
extern unsigned long _timerLeftStartTime;
extern int _timerRightMinutes;
extern int _timerRightSeconds;
extern bool _timerRightRunning;
extern unsigned long _timerRightStartTime;
extern unsigned long _currentTime;

// UI state variables
extern unsigned long lastButtonPress;

// Flush flow state variables
extern bool _flushFlowActive;
extern unsigned long _flushFlowStartTime;
extern bool _initialRightFlushStarted;
extern int _flushCount;
extern bool _leftFlushActive;
extern bool _rightFlushActive;
extern unsigned long _leftFlushStartTime;
extern unsigned long _rightFlushStartTime;

// Animation state structure
struct AnimationState {
  int stage;
  unsigned long lastTime;
  bool active;
};

// Animation states [AnimationType][Location]
extern AnimationState _animStates[3][2]; // 0=Toilet, 1=Camera, 2=WasteRepo

// TFT object
extern TFT_eSPI tft;

// LCD object
extern LiquidCrystal_I2C lcd;

// Shape pointers - declared in main file
extern Shape *_saniLogoShape;
extern Shape *_startStopButtonShape;
extern Shape *_toiletLeftShape;
extern Shape *_toiletRightShape;
extern Shape *_cameraLeftShape;
extern Shape *_cameraRightShape;
extern Shape *_wasteRepoLeftShape;
extern Shape *_wasteRepoRightShape;
extern Shape *_hamburgerShape;

// WiFi/HTTP object management
extern HTTPClient* httpClient;
extern bool wifiNeedsRecreation;
extern unsigned long lastRightCameraCapture;

// Simplified memory snapshot for ESP-IDF analysis
struct MemorySnapshot {
  size_t freeHeap;
  size_t minFreeHeap;
  size_t trackedObjects;
  unsigned long timestamp;
  
  void capture();
  void compare(MemorySnapshot& previous);
};
extern int completedWorkflowCycles;
extern bool firstAnalysisComplete;
extern unsigned long lastMemoryAnalysis;

// Location enum
enum Location
{
  Left,
  Right
};

#endif // GLOBAL_VARS_H