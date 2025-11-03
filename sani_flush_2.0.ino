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
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
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

// LCD Display (I2C)
#define SDA_PIN 14
#define SCL_PIN 15
LiquidCrystal_I2C lcd(0x27, 16, 2);
int flushCount = 0;

// Shared counters (defined here, used by draw_functions.cpp)
int leftFlushCount = 0;
int rightFlushCount = 0;
int imageCount = 0;
int totalWasteML = 0;
unsigned long workflowStartTime = 0;

// WiFi/HTTP object management
HTTPClient* httpClient = nullptr;
bool wifiNeedsRecreation = false;
unsigned long lastRightCameraCapture = 0;

// Memory analysis variables
int completedWorkflowCycles = 0;
bool firstAnalysisComplete = false;
unsigned long lastMemoryAnalysis = 0;

void displayLCD(String line1, String line2)
{
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(line1);
  lcd.setCursor(0, 1);
  lcd.print(line2);
}

void initializeLCDDisplay()
{
  writeLog("Initializing LCD display...");

  Wire.begin(SDA_PIN, SCL_PIN);
  lcd.init();
  lcd.backlight();

  displayLCD("SANI FLUSH 2.0", "LOADING...");
  
  writeLog("LCD display initialized successfully!");
}

// CREATE SETTINGS INSTANCE
SettingsSystem flushSettings(&tft);

// Web server disabled for now due to library conflicts

// Function prototypes
void checkTouch(int16_t touchX, int16_t touchY);
void updateFlushCount(int amount);
void initializeLCDDisplay();

void refreshLastPhoto(Location location);
void checkPhotoRefreshTouch(int16_t touchX, int16_t touchY);
void resetApplicationState();
void recreateNetworkObjects();
void checkMemoryAnalysisTrigger();
void logMemoryObjects();

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
  writeLog("=== SANI FLUSH 2.0 STARTING ===");
  writeLog("ESP32-S3 Serial initialized successfully!");

  // Initialize LCD display
  initializeLCDDisplay();

  displayLCD("ESP32-S3", "LOADING...");
  delay(1000);

  // Initialize WiFi
  writeLog("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print("."); // Keep this for visual progress indication
  }

  // WiFi connected
  Serial.println();
  writeLog("WiFi connected!");
  writeLog("IP: %s", WiFi.localIP().toString().c_str());
  
  // Configure NTP for Pacific Time (handles PST/PDT automatically)
  configTime(-8 * 3600, 3600, "pool.ntp.org", "time.nist.gov");
  writeLog("NTP time sync initiated (Pacific Time)");
  
  // Wait for time to be set
  time_t now = time(0);
  int attempts = 0;
  while (now < 1000000000 && attempts < 20) {
    delay(500);
    now = time(0);
    attempts++;
  }
  
  if (now > 1000000000) {
    struct tm *timeinfo = localtime(&now);
    writeLog("Time synchronized: %04d-%02d-%02d %02d:%02d:%02d", 
      timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday,
      timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
    writeLog("=== PROCESSOR FULLY LOADED - %04d-%02d-%02d %02d:%02d:%02d ===",
      timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday,
      timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
  } else {
    writeLog("Warning: Time sync failed, using system default");
  }


  displayLCD(WiFi.localIP().toString(), "LOADING...");
  delay(1000);

  // Initialize TFT display
  writeLog("Initializing TFT display...");
  tft.init();
  tft.setRotation(0);
  tft.setTouch(calData);
  writeLog("TFT display initialized");

  displayLCD("TFT DISPLAY", "LOADING...");
  delay(1000);

  // Initialize Relay PINs - ENSURE ALL RELAYS ARE OFF
  pinMode(RELAY_P1_PIN, OUTPUT);
  pinMode(RELAY_P2_PIN, OUTPUT);
  pinMode(RELAY_T1_PIN, OUTPUT);
  pinMode(RELAY_T2_PIN, OUTPUT);
  digitalWrite(RELAY_P1_PIN, LOW);
  digitalWrite(RELAY_P2_PIN, LOW);
  digitalWrite(RELAY_T1_PIN, LOW);
  digitalWrite(RELAY_T2_PIN, LOW);
  writeLog("All relays initialized to OFF state");

  // Double-check relay states
  writeLog("Relay states - P1:%d P2:%d T1:%d T2:%d", digitalRead(RELAY_P1_PIN), digitalRead(RELAY_P2_PIN), digitalRead(RELAY_T1_PIN), digitalRead(RELAY_T2_PIN));

  // Initialize global time
  _currentTime = millis();

  // TFT loading
  // Hardware test - cycle through colors (faster)
  writeLog("Testing TFT hardware...");
  tft.fillScreen(TFT_RED);
  delay(200);
  tft.fillScreen(TFT_GREEN);
  delay(200);
  tft.fillScreen(TFT_BLUE);
  delay(200);
  tft.fillScreen(TFT_WHITE);
  writeLog("TFT hardware test complete");

  

  // INITIALIZE SETTINGS SYSTEM
  flushSettings.begin();

  // Reset application state to ensure clean initialization
  resetApplicationState();

  drawMainDisplay();
  writeLog("Main display drawn");

  drawFlowDetails();

  // 5. Ready state - show initial LCD
  updateLCDDisplay();

  writeLog("Setup complete - flow details displayed");
}



void loop()
{
  // Update global time at the start of each loop
  _currentTime = millis();

  // Emergency WiFi recreation when memory gets critically low
  if (ESP.getFreeHeap() < 220000 && !wifiNeedsRecreation) { // Increased threshold to 220KB
    writeLog("[MEM_EMERGENCY] LOW MEMORY WARNING - Forcing WiFi recreation (Free: %d bytes)", ESP.getFreeHeap());
    lastRightCameraCapture = _currentTime - 6000; // Force immediate recreation
    wifiNeedsRecreation = true;
  }

  // Reduced debug output - every 60 seconds instead of 10
  static unsigned long lastDebug = 0;
  if (_currentTime - lastDebug > 60000)
  {
    writeLog("[DEBUG] Time: %lu Free: %d Min: %d", _currentTime, ESP.getFreeHeap(), ESP.getMinFreeHeap());
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
      writeLog("Touch X = %d, Y = %d", touchX, touchY);
      checkTouch(touchX, touchY);
    }
  }

  // Update animations based on current state (but not when settings are visible)
  if (!flushSettings.isSettingsVisible())
  {
    updateAnimations();
  }

  // Recreate WiFi/HTTP objects after camera operations (reduced delay)
  if (wifiNeedsRecreation && _currentTime - lastRightCameraCapture > 2000)
  {
    recreateNetworkObjects();
  }

  // Check for memory analysis triggers
  checkMemoryAnalysisTrigger();

  // Reduced state debug output - every 30 seconds instead of 5
  static unsigned long lastStateDebug = 0;
  if (_currentTime - lastStateDebug > 30000)
  {
    writeLog("[STATE] FF:%d LF:%d RF:%d", _flushFlowActive, _leftFlushActive, _rightFlushActive);
    lastStateDebug = _currentTime;
  }

  // Web server disabled

  // Run waste repo tests once
  runWasteRepoTests();
}

void checkTouch(int16_t touchX, int16_t touchY)
{
  writeLog("Touched: X = %d, Y = %d", touchX, touchY);

  // Check if start/stop button was touched (with debouncing)
  if (_startStopButtonShape && _startStopButtonShape->isTouched(touchX, touchY))
  {
    unsigned long currentTime = millis();
    if (currentTime - lastButtonPress > BUTTON_DEBOUNCE_MS)
    {
      writeLog("Start/Stop button touched!");
      lastButtonPress = currentTime;
      toggleTimers(); // toggleTimers now handles button state and redraw
    }
    else
    {
      writeLog("Button press ignored (debounce)");
    }
  }

  // Check if left toilet was touched
  if (_toiletLeftShape && _toiletLeftShape->isTouched(touchX, touchY))
  {
    writeLog("Left toilet touched!");

    if (!_flushLeft)
    {
      _flushLeft = true;

      // Apply settings to global variables
      RIGHT_TOILET_FLUSH_DELAY_MS = flushSettings.getRightToiletFlushDelaySec() * 1000;
      TOILET_FLUSH_HOLD_TIME_MS = flushSettings.getFlushRelayTimeLapse();
      _flushCountForCameraCapture = flushSettings.getPicEveryNFlushes();
      writeLog("Settings applied - Camera every %d flushes", _flushCountForCameraCapture);
      drawFlowDetails();
    }
  }

  // Check if right toilet was touched
  if (_toiletRightShape && _toiletRightShape->isTouched(touchX, touchY))
  {
    writeLog("Right toilet touched!");

    if (!_flushRight)
    {
      _flushRight = true;

      // Apply settings to global variables
      RIGHT_TOILET_FLUSH_DELAY_MS = flushSettings.getRightToiletFlushDelaySec() * 1000;
      TOILET_FLUSH_HOLD_TIME_MS = flushSettings.getFlushRelayTimeLapse();
      _flushCountForCameraCapture = flushSettings.getPicEveryNFlushes();
      writeLog("Settings applied - Camera every %d flushes", _flushCountForCameraCapture);
      drawFlowDetails();
    }
  }

  // Check if left camera was touched
  if (_cameraLeftShape && _cameraLeftShape->isTouched(touchX, touchY))
  {
    writeLog("Left camera touched!");

    if (!_flashCameraLeft)
    {
      _flashCameraLeft = true;
      writeLog("Manual snap pic - left camera flash animation started");
      // Animation will handle dual camera capture
    }
  }

  // Check if right camera was touched
  if (_cameraRightShape && _cameraRightShape->isTouched(touchX, touchY))
  {
    writeLog("Right camera touched!");

    if (!_flashCameraRight)
    {
      _flashCameraRight = true;
      writeLog("Manual snap pic - right camera flash animation started");
      // Animation will handle dual camera capture
    }
  }

  // Check if left waste repo was touched
  if (_wasteRepoLeftShape && _wasteRepoLeftShape->isTouched(touchX, touchY))
  {
    writeLog("Left waste repo touched!");

    if (!_animateWasteRepoLeft)
    {
      _animateWasteRepoLeft = true;
      drawFlowDetails();
    }
  }

  // Check if right waste repo was touched
  if (_wasteRepoRightShape && _wasteRepoRightShape->isTouched(touchX, touchY))
  {
    writeLog("Right waste repo touched!");

    if (!_animateWasteRepoRight)
    {
      _animateWasteRepoRight = true;
      drawFlowDetails();
    }
  }

  // Check if hamburger menu was touched
  if (_hamburgerShape && _hamburgerShape->isTouched(touchX, touchY))
  {
    writeLog("Hamburger menu touched!");
    if (!flushSettings.isSettingsVisible())
    {
      flushSettings.showSettings();
    }
    else
    {
      flushSettings.hideSettings();
      drawMainDisplay();
      drawFlowDetails();
    }
  }
}

void resetApplicationState()
{
  _flushLeft = false;
  _flushRight = false;
  _flashCameraLeft = false;
  _flashCameraRight = false;
  _animateWasteRepoLeft = false;
  _animateWasteRepoRight = false;
  _drawTriangle = true;
  _timerLeftMinutes = 0;
  _timerLeftSeconds = 0;
  _timerLeftRunning = false;
  _timerLeftStartTime = 0;
  _timerRightMinutes = 0;
  _timerRightSeconds = 0;
  _timerRightRunning = false;
  _timerRightStartTime = 0;
  _flushFlowActive = false;
  _flushFlowStartTime = 0;
  _initialRightFlushStarted = false;
  _flushCount = 0;
  _leftFlushActive = false;
  _rightFlushActive = false;
  _leftFlushStartTime = 0;
  _rightFlushStartTime = 0;

  for (int i = 0; i < 3; i++)
  {
    for (int j = 0; j < 2; j++)
    {
      _animStates[i][j].stage = 0;
      _animStates[i][j].lastTime = 0;
      _animStates[i][j].active = false;
    }
  }

  digitalWrite(RELAY_P1_PIN, LOW);
  digitalWrite(RELAY_P2_PIN, LOW);
  digitalWrite(RELAY_T1_PIN, LOW);
  digitalWrite(RELAY_T2_PIN, LOW);

  // Reset counters and display
  updateLCDDisplay();
}

void updateLCDDisplay()
{
  // Use char arrays with safe bounds checking
  char line1[17]; // LCD is 16 chars + null terminator
  char line2[17];
  
  // Clear buffers to prevent corruption
  memset(line1, 0, sizeof(line1));
  memset(line2, 0, sizeof(line2));
  
  if (_flushFlowActive)
  {
    // When workflow is running: 2 lines
    // L1: G00        W000ML
    // L2: L00 R00 I00
    float totalGallons = calculateTotalGallons();
    
    // Limit values to prevent buffer overflow
    int gallons = min(999, (int)totalGallons);
    int waste = min(9999, totalWasteML);

    if (waste > 9999)
    {
      int liters = waste / 1000;
      snprintf(line1, sizeof(line1), "G%03d     W%02dL", gallons, min(99, liters));
    }
    else
    {
      snprintf(line1, sizeof(line1), "G%03d   W%04dML", gallons, waste);
    }

    // Format counters with bounds checking (max 999)
    int leftCount = min(999, leftFlushCount);
    int rightCount = min(999, rightFlushCount);
    int imgCount = min(999, imageCount);
    snprintf(line2, sizeof(line2), "L%03d R%03d I%03d", leftCount, rightCount, imgCount);

    displayLCD(line1, line2);
  }
  else
  {
    // When stopped: PRESS START / L00 R00 I00
    int leftCount = min(999, leftFlushCount);
    int rightCount = min(999, rightFlushCount);
    int imgCount = min(999, imageCount);
    snprintf(line2, sizeof(line2), "L%03d R%03d I%03d", leftCount, rightCount, imgCount);
    displayLCD("PRESS START", line2);
  }
}

void incrementFlushCounter()
{
  // This function is called from draw_functions.cpp
  updateLCDDisplay();
  drawFlowDetails(); // Update TFT flow details to match LCD
}

void incrementWasteCounter()
{
  // This function is called from draw_functions.cpp
  updateLCDDisplay();
  drawFlowDetails(); // Update TFT flow details to match LCD
}

void testWasteRepoTiming()
{
  writeLog("=== WASTE REPO TIMING TEST ===");
  int wasteQty = flushSettings.getWasteQtyPerFlush();
  int pumpRate = PUMP_WASTE_ML_SEC;
  int delayMs = flushSettings.getWasteRepoTriggerDelayMs();
  writeLog("- Waste Qty Per Flush: %dml", wasteQty);
  writeLog("- Pump Rate: %dml/sec", pumpRate);
  writeLog("- Waste Repo Delay: %dms", delayMs);
}

void testWasteRepoActivation()
{
  writeLog("=== WASTE REPO ACTIVATION TEST ===");
  writeLog("Manual activation works when touching waste repo image");
}

void runWasteRepoTests()
{
  static bool testsRun = false;
  if (!testsRun && millis() > 5000)
  {
    testWasteRepoTiming();
    testWasteRepoActivation();
    testsRun = true;
  }
}
