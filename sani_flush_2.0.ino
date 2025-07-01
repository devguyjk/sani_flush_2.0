// Complete working main code with settings integration
#include <TFT_eSPI.h>
#include "Shapes.h"
#include <SPI.h>
#include <TM1637Display.h>
#include "global_vars.h"
#include "draw_functions.h"
#include "settings_system.h"  

uint16_t calData[5] = {237, 3595, 372, 3580, 4};

// RELAY SWITCH PINS
#define RELAY_P1_PIN 35
#define RELAY_P2_PIN 36
#define RELAY_T1_PIN 37
#define RELAY_T2_PIN 38
// 4 Digit Display
#define TM1637_SCK 16
#define TM1637_DIO 17

// TM1637 Display object
TM1637Display display(TM1637_SCK, TM1637_DIO);
int flushCount = 0;

// CREATE SETTINGS INSTANCE
SettingsSystem flushSettings(&tft);

// Function prototypes
void checkTouch(int16_t touchX, int16_t touchY);
void updateFlushCount(int amount);
void initializeDisplay();

void setup()
{
  Serial.begin(115200);

  tft.init();
  tft.setTouch(calData);
  tft.setRotation(0);
  Serial.println("tft Loaded");

  // Initialize Relay PINs
  pinMode(RELAY_P1_PIN, OUTPUT);
  pinMode(RELAY_P2_PIN, OUTPUT);
  pinMode(RELAY_T1_PIN, OUTPUT);
  pinMode(RELAY_T2_PIN, OUTPUT);
  digitalWrite(RELAY_P1_PIN, LOW);
  digitalWrite(RELAY_P2_PIN, LOW);
  digitalWrite(RELAY_T1_PIN, LOW);
  digitalWrite(RELAY_T2_PIN, LOW);
  Serial.println("relays setup");

  // Initialize global time
  _currentTime = millis();

  tft.fillScreen(TFT_WHITE);
  Serial.println("TFT screen filled with white");
  
  // INITIALIZE SETTINGS SYSTEM
  flushSettings.begin();
  
  drawMainDisplay();
  Serial.println("Main display drawn");
  
  initializeDisplay();
}

void loop() {
  // Update global time at the start of each loop
  _currentTime = millis();

  // HANDLE SETTINGS TOUCH FIRST
  if(flushSettings.isSettingsVisible()) {
    flushSettings.handleTouch();
    // After settings touch, check if we need to redraw main
    if(!flushSettings.isSettingsVisible()) {
      drawMainDisplay(); // Redraw main when settings close
    }
  } else {
    // Only handle main touches when settings are not visible
    uint16_t touchX, touchY;
    if (tft.getTouch(&touchX, &touchY)) {
      Serial.print("Touch X = ");
      Serial.print(touchX);
      Serial.print(", Y = ");
      Serial.println(touchY);

      checkTouch(touchX, touchY);
    }
  }

  // Update animations based on current state
  updateAnimations();
}

void checkTouch(int16_t touchX, int16_t touchY)
{
  Serial.println("Touched: X = " + String(touchX) + ", Y = " + String(touchY));

  // Check if start/stop button was touched
  if (_startStopButtonShape && _startStopButtonShape->isTouched(touchX, touchY))
  {
    Serial.println("Start/Stop button touched!");
    _drawTriangle = !_drawTriangle;
    toggleTimers();
    drawStartStopButton();
  }
  
  // Check if left toilet was touched
  if (_toiletLeftShape && _toiletLeftShape->isTouched(touchX, touchY))
  {
    Serial.println("Left toilet touched!");
    if (!_flushLeft) {
      _flushLeft = true;
      
      // USE SETTINGS: Get delay time from settings
      int delayTime = flushSettings.getFlushDelayTime();
      Serial.println("Using flush delay: " + String(delayTime) + "ms");
      // Apply your delay logic here
    }
  }
  
  // Check if right toilet was touched
  if (_toiletRightShape && _toiletRightShape->isTouched(touchX, touchY))
  {
    Serial.println("Right toilet touched!");
    if (!_flushRight) {
      _flushRight = true;
      
      // USE SETTINGS: Get time gap from settings
      int timeGap = flushSettings.getFlushTimeGap();
      Serial.println("Using flush time gap: " + String(timeGap) + "ms");
      // Apply your timing logic here
    }
  }
  
  // Check if left camera was touched
  if (_cameraLeftShape && _cameraLeftShape->isTouched(touchX, touchY))
  {
    Serial.println("Left camera touched!");
    if (!_flashCameraLeft) {
      _flashCameraLeft = true;
      
      // USE SETTINGS: Check if snap pic is enabled
      if(flushSettings.getFlushToSnapPic()) {
        Serial.println("Snap pic enabled - taking photo");
        // Your photo logic here
      }
    }
  }
  
  // Check if right camera was touched
  if (_cameraRightShape && _cameraRightShape->isTouched(touchX, touchY))
  {
    Serial.println("Right camera touched!");
    if (!_flashCameraRight) {
      _flashCameraRight = true;
      
      // USE SETTINGS: Check snap pic setting
      if(flushSettings.getFlushToSnapPic()) {
        Serial.println("Snap pic enabled - taking photo");
        // Your photo logic here
      }
    }
  }
  
  // Check if left waste repo was touched
  if (_wasteRepoLeftShape && _wasteRepoLeftShape->isTouched(touchX, touchY))
  {
    Serial.println("Left waste repo touched!");
    if (!_animateWasteRepoLeft) {
      _animateWasteRepoLeft = true;
      
      // USE SETTINGS: Get waste quantity
      int wasteQty = flushSettings.getWasteQtyPerFlush();
      Serial.println("Waste quantity per flush: " + String(wasteQty) + "oz");
      // Apply your waste calculation logic here
    }
  }
  
  // Check if right waste repo was touched
  if (_wasteRepoRightShape && _wasteRepoRightShape->isTouched(touchX, touchY))
  {
    Serial.println("Right waste repo touched!");
    if (!_animateWasteRepoRight) {
      _animateWasteRepoRight = true;
      
      // USE SETTINGS: Get waste quantity
      int wasteQty = flushSettings.getWasteQtyPerFlush();
      Serial.println("Waste quantity per flush: " + String(wasteQty) + "oz");
      // Apply your waste calculation logic here
    }
  }
  
  // HAMBURGER MENU TOUCH HANDLING
  if (_hamburgerShape && _hamburgerShape->isTouched(touchX, touchY))
  {
    Serial.println("Hamburger menu touched!");
    
    if(flushSettings.isSettingsVisible()) {
      // If settings are visible, hide them and return to main
      flushSettings.hideSettings();
      drawMainDisplay(); // Redraw your main display
    } else {
      // Show settings
      flushSettings.showSettings();
    }
  }
}

void initializeDisplay()
{
  Serial.println("Initializing TM1637 display...");
  display.setBrightness(0x07);
  delay(50);
  display.showNumberDec(flushCount, false);
  delay(50);
  Serial.println("TM1637 display initialized");
}

void updateFlushCount(int amount)
{
  flushCount += amount;
  if (flushCount < 0) flushCount = 0;
  display.showNumberDec(flushCount, false);
  Serial.print("Flush count updated to: ");
  Serial.println(flushCount);
}