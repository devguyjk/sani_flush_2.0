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
const uint16_t BTN_TRIANGLE_COLOR = TFT_GREEN;
const uint16_t BTN_SQUARE_COLOR = TFT_RED;
int RIGHT_TOILET_FLUSH_DELAY_MS = 10000; // 10 seconds delay for right toilet timer

// Animation timing constants
const int TOILET_ANIM_STAGE_DURATION_MS = 500;
const int CAMERA_FLASH_STAGE_DURATION_MS = 100;
const int WASTE_REPO_ANIM_STAGE_DURATION_MS = 400;
const int TOILET_ANIM_TOTAL_STAGES = 4;
const int CAMERA_FLASH_TOTAL_STAGES = 5;
const int WASTE_REPO_ANIM_TOTAL_STAGES = 5; // 5 steps: 01->02->03->04->01

// Relay timing constants
const int PUMP_WASTE_ML_SEC = 25; // ML per second pump rate
int TOILET_FLUSH_HOLD_TIME_MS = 2000; // 2 seconds

// Flush flow variables (configurable) - now use settings
int _flushCountForCameraCapture = 3; // Camera triggers every N flushes - will be updated from settings

// GLOBAL VARIABLES (prefixed with _)
bool _flushLeft = false;
bool _flushRight = false;
bool _flashCameraLeft = false;
bool _flashCameraRight = false;
bool _debugPrintShapeDetails = false; // Set to true to print shape details on creation
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
bool _initialRightFlushStarted = false;
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

// Camera and server configuration
const char* left01CameraID = "5f96fe52";
const char* left02CameraID = "5e80ed5f";
const char* right01CameraID = "c4df83c4";
const char* right02CameraID = "2ddb4fdb";
const char* uploadServerURL = "http://192.168.4.103:5000/uploadSaniPhoto";

// Hardware pin definitions
const int RELAY_P1_PIN = 35;
const int RELAY_P2_PIN = 36;
const int RELAY_T1_PIN = 37;
const int RELAY_T2_PIN = 38;

// UI constants
const unsigned long BUTTON_DEBOUNCE_MS = 300;

// UI state variables
unsigned long lastButtonPress = 0;

// TFT object
TFT_eSPI tft = TFT_eSPI();

// LCD object (declared in main file)
// LiquidCrystal_I2C lcd(0x27, 16, 2);

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