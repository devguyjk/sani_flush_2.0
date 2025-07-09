#include "global_vars.h"

// Global Constants (ALL_CAPS_WITH_UNDERSCORES)
const int DEFAULT_PADDING = 5;
const int CAMERA_HEIGHT = 20;
const int CAMERA_WIDTH = 50;
const int TOILET_HEIGHT = 105;
const int TOILET_WIDTH = 85;
const int SCREEN_WIDTH = 240;
const int SCREEN_HEIGHT = 320;
const int LOGO_WIDTH = 165;
const int LOGO_HEIGHT = 40;
const int WASTE_REPO_HEIGHT = 75;
const int WASTE_REPO_WIDTH = 25;
const uint16_t BTN_TRIANGLE_COLOR = ILI9341_GREEN;
const uint16_t BTN_SQUARE_COLOR = ILI9341_RED;
const int RIGHT_TOILET_FLUSH_DELAY_MS = 10000; // 10 seconds delay for right toilet timer

// Animation timing constants
const int TOILET_ANIM_STAGE_DURATION_MS = 500;
const int CAMERA_FLASH_STAGE_DURATION_MS = 100;
const int WASTE_REPO_ANIM_STAGE_DURATION_MS = 400;
const int TOILET_ANIM_TOTAL_STAGES = 4;
const int CAMERA_FLASH_TOTAL_STAGES = 5;
const int WASTE_REPO_ANIM_TOTAL_STAGES = 5; // 5 steps: 01->02->03->04->01

// Relay timing constants
int _pumpWasteMl = 100; // ML of waste to pump
const int PUMP_WASTE_ML_SEC = 50; // ML per second pump rate
const int PUMP_RELAY_ACTIVE_TIME_MS = (_pumpWasteMl * 1000) / PUMP_WASTE_ML_SEC; // 2000ms
const int TOILET_FLUSH_HOLD_TIME_MS = 2000; // 2 seconds

// Flush flow variables (configurable)
int _flushTotalDurationMs = 60000; // 60 seconds total flush time
int _wasteRepoTriggerDelayMs = 5000; // 5 seconds after flush starts
int _cameraTriggerAfterFlushMs = 2500; // 0.5 * waste repo trigger delay
int _flushCountForCamera = 3; // Camera triggers every 3 flushes

// GLOBAL VARIABLES (prefixed with _)
bool _flushLeft = false;
bool _flushRight = false;
bool _flashCameraLeft = false;
bool _flashCameraRight = false;
bool _animateWasteRepoLeft = false;
bool _animateWasteRepoRight = false;
bool _drawTriangle = true;
int _timerLeftMinutes = 0;
int _timerLeftSeconds = 0;
bool _timerLeftRunning = false;
unsigned long _timerLeftStartTime = 0;
int _timerRightMinutes = 0;
int _timerRightSeconds = 0;
bool _timerRightRunning = false;
unsigned long _timerRightStartTime = 0;
unsigned long _currentTime = 0; // Global time for all animations

// Flush flow state variables
bool _flushFlowActive = false;
unsigned long _flushFlowStartTime = 0;
int _flushCount = 0;
bool _leftFlushActive = false;
bool _rightFlushActive = false;
unsigned long _leftFlushStartTime = 0;
unsigned long _rightFlushStartTime = 0;

// Animation states [AnimationType][Location] - 0=Toilet, 1=Camera, 2=WasteRepo
AnimationState _animStates[3][2] = {
  {{0, 0, false}, {0, 0, false}}, // Toilet Left/Right
  {{0, 0, false}, {0, 0, false}}, // Camera Left/Right  
  {{0, 0, false}, {0, 0, false}}  // WasteRepo Left/Right
};

// TFT object
TFT_eSPI tft = TFT_eSPI();
// ================== Global Shape Objects ==================
Shape *_saniLogoShape = nullptr;
Shape *_startStopButtonShape = nullptr;
Shape *_toiletLeftShape = nullptr;
Shape *_toiletRightShape = nullptr;
Shape *_cameraLeftShape = nullptr;
Shape *_cameraRightShape = nullptr;
Shape *_wasteRepoLeftShape = nullptr;
Shape *_wasteRepoRightShape = nullptr;
Shape *_hamburgerShape = nullptr;