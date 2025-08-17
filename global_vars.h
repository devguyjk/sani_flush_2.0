#ifndef GLOBAL_VARS_H
#define GLOBAL_VARS_H

#include <TFT_eSPI.h>

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
extern const int WASTE_REPO_ANIM_DELAY;
extern int RIGHT_TOILET_FLUSH_DELAY_MS;

// Camera and server configuration
extern const char* left01CameraID;
extern const char* left02CameraID;
extern const char* right01CameraID;
extern const char* right02CameraID;
extern const char* uploadServerURL;
extern String left01CameraCaptureUrl;
extern String left02CameraCaptureUrl;
extern String right01CameraCaptureUrl;
extern String right02CameraCaptureUrl;

// Hardware pin definitions
extern const int RELAY_P1_PIN;
extern const int RELAY_P2_PIN;
extern const int RELAY_T1_PIN;
extern const int RELAY_T2_PIN;
extern const int TM1637_SCK;
extern const int TM1637_DIO;

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

// Location enum
enum Location
{
  Left,
  Right
};

#endif // GLOBAL_VARS_H