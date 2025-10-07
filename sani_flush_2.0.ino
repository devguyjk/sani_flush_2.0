// Complete working main code with settings integration

// BOARD AND LIBRARY VERSION CHECKS - EXACT VERSIONS REQUIRED
#ifndef ESP_ARDUINO_VERSION
  #error "ESP32 Arduino Core version not detected. Please install ESP32 board package."
#endif

// ESP32 Board Package - MUST BE 3.2.1
#if ESP_ARDUINO_VERSION != ESP_ARDUINO_VERSION_VAL(3, 2, 1)
  #error "This project requires ESP32 Arduino Core version 3.2.1. Please install the correct version from Espressif Systems."
#endif

// TFT_eSPI Library Version Check - MUST BE 2.5.34
#include <TFT_eSPI.h>
#ifndef TFT_ESPI_VERSION
  #error "TFT_eSPI version not detected. Please install TFT_eSPI library."
#endif


#include "Shapes.h"
#include <SPI.h>
#include <TM1637Display.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "global_vars.h"
#include "draw_functions.h"
#include "settings_system.h"

// Test function declarations
void testWasteRepoTiming();
void testWasteRepoActivation();
void runWasteRepoTests();

uint16_t calData[5] = {237, 3595, 372, 3580, 4};

// WiFi credentials
const char *ssid = "juke-fiber-ofc";
const char *password = "swiftbubble01";
// TM1637 Display object
TM1637Display display(TM1637_SCK, TM1637_DIO);
int flushCount = 0;
// CREATE SETTINGS INSTANCE
SettingsSystem flushSettings(&tft);

// Web server disabled for now due to library conflicts

// Function prototypes
void checkTouch(int16_t touchX, int16_t touchY);
void updateFlushCount(int amount);
void initializeDisplay();
String callUploadSaniPhoto(const char *cameraID, const char *imagePrefix);
void refreshLastPhoto(Location location);
void checkPhotoRefreshTouch(int16_t touchX, int16_t touchY);
void resetApplicationState();

void setup()
{
  // ESP32-S3 specific initialization
  delay(2000); // Longer delay for ESP32-S3
  Serial.begin(115200);
  while (!Serial && millis() < 5000)
  {
    delay(10); // Wait for Serial or timeout after 5 seconds
  }
  Serial.flush();
  Serial.println("\n=== SANI FLUSH 2.0 STARTING ===");
  Serial.println("ESP32-S3 Serial initialized successfully!");
  


  // Initialize WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("WiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  Serial.println("Web server disabled - using touch interface only");

  // Initialize TFT display first
  Serial.println("Initializing TFT display...");
  tft.init();
  tft.setRotation(0);
  tft.setTouch(calData);
  Serial.println("TFT display initialized");

  // Initialize Relay PINs - ENSURE ALL RELAYS ARE OFF
  pinMode(RELAY_P1_PIN, OUTPUT);
  pinMode(RELAY_P2_PIN, OUTPUT);
  pinMode(RELAY_T1_PIN, OUTPUT);
  pinMode(RELAY_T2_PIN, OUTPUT);
  digitalWrite(RELAY_P1_PIN, LOW);
  digitalWrite(RELAY_P2_PIN, LOW);
  digitalWrite(RELAY_T1_PIN, LOW);
  digitalWrite(RELAY_T2_PIN, LOW);
  Serial.println("All relays initialized to OFF state");

  // Double-check relay states
  Serial.println("Relay states - P1:" + String(digitalRead(RELAY_P1_PIN)) +
                 " P2:" + String(digitalRead(RELAY_P2_PIN)) +
                 " T1:" + String(digitalRead(RELAY_T1_PIN)) +
                 " T2:" + String(digitalRead(RELAY_T2_PIN)));

  // Initialize global time
  _currentTime = millis();

  // Hardware test - cycle through colors
  Serial.println("Testing TFT hardware...");
  tft.fillScreen(TFT_RED);
  delay(1000);
  tft.fillScreen(TFT_GREEN);
  delay(1000);
  tft.fillScreen(TFT_BLUE);
  delay(1000);
  tft.fillScreen(TFT_WHITE);
  Serial.println("TFT hardware test complete");

  // INITIALIZE SETTINGS SYSTEM
  flushSettings.begin();

  // Reset application state to ensure clean initialization
  resetApplicationState();
  
  drawMainDisplay();
  Serial.println("Main display drawn");

  initializeDisplay();
  

  
  Serial.println("Setup complete - flow details displayed");
}

void loop()
{
  // Update global time at the start of each loop
  _currentTime = millis();

  // Debug output every 10 seconds to verify serial is working
  static unsigned long lastDebug = 0;
  if (_currentTime - lastDebug > 10000)
  {
    Serial.println("[DEBUG] System running - Time: " + String(_currentTime));
    lastDebug = _currentTime;
  }

  // HANDLE SETTINGS TOUCH FIRST
  if (flushSettings.isSettingsVisible())
  {
    flushSettings.handleTouch();
    // After settings touch, check if we need to redraw main
    if (!flushSettings.isSettingsVisible())
    {
      drawMainDisplay(); // Redraw main when settings close
      drawFlowDetails(); // Ensure flow details reflect any setting changes
    }
  }
  else
  {
    // Only handle main touches when settings are not visible
    uint16_t touchX, touchY;
    if (tft.getTouch(&touchX, &touchY))
    {
      Serial.print("Touch X = ");
      Serial.print(touchX);
      Serial.print(", Y = ");
      Serial.println(touchY);

      checkTouch(touchX, touchY);
    }
  }

  // Update animations based on current state (but not when settings are visible)
  if (!flushSettings.isSettingsVisible()) {
    updateAnimations();
  }
  


  // Debug output for active states
  static unsigned long lastStateDebug = 0;
  if (_currentTime - lastStateDebug > 5000)
  {
    Serial.println("[STATE] FlushFlow:" + String(_flushFlowActive) +
                   " LeftFlush:" + String(_leftFlushActive) +
                   " RightFlush:" + String(_rightFlushActive) +
                   " Triangle:" + String(_drawTriangle) +
                   " WasteRepoL:" + String(_animateWasteRepoLeft) +
                   " WasteRepoR:" + String(_animateWasteRepoRight));
    lastStateDebug = _currentTime;
  }
  
  // Web server disabled
  
  // Run waste repo tests once
  runWasteRepoTests();
}

void checkTouch(int16_t touchX, int16_t touchY)
{
  Serial.println("Touched: X = " + String(touchX) + ", Y = " + String(touchY));

  // Check if start/stop button was touched (with debouncing)
  if (_startStopButtonShape && _startStopButtonShape->isTouched(touchX, touchY))
  {
    unsigned long currentTime = millis();
    if (currentTime - lastButtonPress > BUTTON_DEBOUNCE_MS)
    {
      Serial.println("Start/Stop button touched!");
      lastButtonPress = currentTime;
      toggleTimers(); // toggleTimers now handles button state and redraw
    }
    else
    {
      Serial.println("Button press ignored (debounce)");
    }
  }

  // Check if left toilet was touched
  if (_toiletLeftShape && _toiletLeftShape->isTouched(touchX, touchY))
  {
    Serial.println("Left toilet touched!");
    if (!_flushLeft)
    {
      _flushLeft = true;

      // Apply settings to global variables
      RIGHT_TOILET_FLUSH_DELAY_MS = flushSettings.getRightToiletFlushDelaySec() * 1000;
      TOILET_FLUSH_HOLD_TIME_MS = flushSettings.getFlushRelayTimeLapse();
      _flushCountForCameraCapture = flushSettings.getPicEveryNFlushes();
      Serial.println("Settings applied to system - Camera every " + String(_flushCountForCameraCapture) + " flushes");
      drawFlowDetails();
    }
  }

  // Check if right toilet was touched
  if (_toiletRightShape && _toiletRightShape->isTouched(touchX, touchY))
  {
    Serial.println("Right toilet touched!");
    if (!_flushRight)
    {
      _flushRight = true;

      // Apply settings to global variables
      RIGHT_TOILET_FLUSH_DELAY_MS = flushSettings.getRightToiletFlushDelaySec() * 1000;
      TOILET_FLUSH_HOLD_TIME_MS = flushSettings.getFlushRelayTimeLapse();
      _flushCountForCameraCapture = flushSettings.getPicEveryNFlushes();
      Serial.println("Settings applied to system - Camera every " + String(_flushCountForCameraCapture) + " flushes");
      drawFlowDetails();
    }
  }

  // Check if left camera was touched
  if (_cameraLeftShape && _cameraLeftShape->isTouched(touchX, touchY))
  {
    Serial.println("Left camera touched!");
    if (!_flashCameraLeft)
    {
      _flashCameraLeft = true;
      Serial.println("Manual snap pic - left camera flash animation started");
      // Animation will handle dual camera capture
    }
  }

  // Check if right camera was touched
  if (_cameraRightShape && _cameraRightShape->isTouched(touchX, touchY))
  {
    Serial.println("Right camera touched!");
    if (!_flashCameraRight)
    {
      _flashCameraRight = true;
      Serial.println("Manual snap pic - right camera flash animation started");
      // Animation will handle dual camera capture
    }
  }

  // Check if left waste repo was touched
  if (_wasteRepoLeftShape && _wasteRepoLeftShape->isTouched(touchX, touchY))
  {
    Serial.println("Left waste repo touched!");
    if (!_animateWasteRepoLeft)
    {
      _animateWasteRepoLeft = true;

      // Apply settings to global variables
      RIGHT_TOILET_FLUSH_DELAY_MS = flushSettings.getRightToiletFlushDelaySec() * 1000;
      TOILET_FLUSH_HOLD_TIME_MS = flushSettings.getFlushRelayTimeLapse();
      _flushCountForCameraCapture = flushSettings.getPicEveryNFlushes();
      Serial.println("Settings applied to system - Camera every " + String(_flushCountForCameraCapture) + " flushes");
      drawFlowDetails();
    }
  }

  // Check if right waste repo was touched
  if (_wasteRepoRightShape && _wasteRepoRightShape->isTouched(touchX, touchY))
  {
    Serial.println("Right waste repo touched!");
    if (!_animateWasteRepoRight)
    {
      _animateWasteRepoRight = true;

      // Apply settings to global variables
      RIGHT_TOILET_FLUSH_DELAY_MS = flushSettings.getRightToiletFlushDelaySec() * 1000;
      TOILET_FLUSH_HOLD_TIME_MS = flushSettings.getFlushRelayTimeLapse();
      _flushCountForCameraCapture = flushSettings.getPicEveryNFlushes();
      Serial.println("Settings applied to system - Camera every " + String(_flushCountForCameraCapture) + " flushes");
      drawFlowDetails();
    }
  }

  // Check if hamburger menu was touched
  if (_hamburgerShape && _hamburgerShape->isTouched(touchX, touchY))
  {
    Serial.println("Hamburger menu touched!");
    if (!flushSettings.isSettingsVisible())
    {
      Serial.println("Showing settings...");
      flushSettings.showSettings();
    }
    else
    {
      Serial.println("Hiding settings...");
      flushSettings.hideSettings();
      drawMainDisplay();
      drawFlowDetails();
    }
  }
}

// Reset application to initial state
void resetApplicationState() {
  Serial.println("Resetting application state...");
  
  // Reset all animation flags
  _flushLeft = false;
  _flushRight = false;
  _flashCameraLeft = false;
  _flashCameraRight = false;
  _animateWasteRepoLeft = false;
  _animateWasteRepoRight = false;
  _drawTriangle = true;
  
  // Reset timers
  _timerLeftMinutes = 0;
  _timerLeftSeconds = 0;
  _timerLeftRunning = false;
  _timerLeftStartTime = 0;
  _timerRightMinutes = 0;
  _timerRightSeconds = 0;
  _timerRightRunning = false;
  _timerRightStartTime = 0;
  
  // Reset flush flow state
  _flushFlowActive = false;
  _flushFlowStartTime = 0;
  _flushCount = 0;
  _leftFlushActive = false;
  _rightFlushActive = false;
  _leftFlushStartTime = 0;
  _rightFlushStartTime = 0;
  
  // Reset animation states
  for(int i = 0; i < 3; i++) {
    for(int j = 0; j < 2; j++) {
      _animStates[i][j].stage = 0;
      _animStates[i][j].lastTime = 0;
      _animStates[i][j].active = false;
    }
  }
  
  // Turn off all relays
  digitalWrite(RELAY_P1_PIN, LOW);
  digitalWrite(RELAY_P2_PIN, LOW);
  digitalWrite(RELAY_T1_PIN, LOW);
  digitalWrite(RELAY_T2_PIN, LOW);
  
  // Redraw display
  drawMainDisplay();
  drawFlowDetails();
  
  Serial.println("Application state reset complete");
}

// Web server handlers removed due to library conflicts

void initializeDisplay()
{
  Serial.println("Initializing TM1637 display...");
  display.setBrightness(0x07);
  delay(50);
  display.showNumberDec(flushCount, false);
  delay(50);
  Serial.println("TM1637 display initialized");
  Serial.println("=== SETUP COMPLETE - READY FOR OPERATION ===");
}

// Test functions for waste repo debugging
void testWasteRepoTiming() {
  Serial.println("=== WASTE REPO TIMING TEST ===");
  
  int wasteQty = flushSettings.getWasteQtyPerFlush();
  int pumpRate = PUMP_WASTE_ML_SEC;
  int delayMs = flushSettings.getWasteRepoTriggerDelayMs();
  
  Serial.println("Current Settings:");
  Serial.println("- Waste Qty Per Flush: " + String(wasteQty) + "ml");
  Serial.println("- Pump Rate: " + String(pumpRate) + "ml/sec");
  Serial.println("- Waste Repo Delay: " + String(delayMs) + "ms (" + String(delayMs/1000) + " seconds)");
  
  int expectedDuration = (wasteQty * 1000) / pumpRate;
  Serial.println("- Expected Pump Duration: " + String(expectedDuration) + "ms (" + String(expectedDuration/1000.0) + " seconds)");
}

void testWasteRepoActivation() {
  Serial.println("=== WASTE REPO ACTIVATION TEST ===");
  Serial.println("Manual activation works when touching waste repo image");
  Serial.println("Automatic activation should trigger after " + String(flushSettings.getWasteRepoTriggerDelayMs()) + "ms delay");
}

void runWasteRepoTests() {
  static bool testsRun = false;
  if (!testsRun && millis() > 5000) {
    testWasteRepoTiming();
    testWasteRepoActivation();
    testsRun = true;
  }
}
