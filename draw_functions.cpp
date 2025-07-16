#include "draw_functions.h"
#include <TM1637Display.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <TJpg_Decoder.h>
#include "sani_flush_logo_165x40.h"
#include "Toilet_Full_85x105_01.h"
#include "Toilet_Full_85x105_02.h"
#include "Toilet_Full_85x105_03.h"
#include "Toilet_Full_85x105_04.h"
#include "Toilet_Full_85x105_05.h"
#include "Waste_Repo_75x25_01.h"
#include "Waste_Repo_75x25_02.h"
#include "Waste_Repo_75x25_03.h"
#include "Waste_Repo_75x25_04.h"

// Animation type constants
enum AnimationType
{
  TOILET = 0,
  CAMERA = 1,
  WASTE_REPO = 2
};

// External references
extern TM1637Display display;
extern int flushCount;
extern const char *leftCameraID;
extern const char *rightCameraID;
extern const char *uploadServerURL;
extern String leftCameraCaptureUrl;
extern String rightCameraCaptureUrl;
extern const unsigned long BUTTON_DEBOUNCE_MS;
extern unsigned long lastButtonPress;

// Function prototypes

// Logging function for workflow steps
void logStep(const char *step)
{
  unsigned long timestamp = millis();
  Serial.print("[T:");
  Serial.print(timestamp);
  Serial.print("] ");
  Serial.println(step);
}

String callUploadSaniPhoto(const char *cameraID, const char *imagePrefix)
{
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("WiFi not connected!");
    return "Error: WiFi not connected";
  }

  HTTPClient http;
  String url = String(uploadServerURL) + "?camera_id=" + String(cameraID) + "&image_prefix=" + String(imagePrefix);
  Serial.println("[HTTP] Connecting to: " + url);

  http.begin(url);
  http.setTimeout(15000);

  int httpResponseCode = http.POST("");
  String response = "";

  Serial.println("[HTTP] Response code: " + String(httpResponseCode));

  if (httpResponseCode > 0)
  {
    response = http.getString();
    Serial.println("[HTTP] Response body: " + response);
  }
  else
  {
    Serial.println("[HTTP] Connection failed: " + String(httpResponseCode));
    response = "Connection failed: " + String(httpResponseCode);
  }

  http.end();
  return response;
}

void updateFlushCount(int amount)
{
  if (amount != 0)
  {
    flushCount += amount;
    if (flushCount < 0)
      flushCount = 0;
  }
  else
  {
    flushCount = _flushCount; // Sync with global counter
  }
  display.showNumberDec(flushCount, false);
  if (amount != 0)
  {
    logStep((String("Display updated - Flush count: ") + String(flushCount)).c_str());
  }
}

// Static variables to track timer display updates
static unsigned long _lastTimerUpdate = 0;
static int _lastLeftSeconds = -1;
static int _lastRightSeconds = -1;
static bool wasteRepoCompletedLeft = false;
static bool wasteRepoCompletedRight = false;
static AnimationType lastAnimationLeft = TOILET;
static AnimationType lastAnimationRight = TOILET;

// One-time trigger flags to prevent re-triggering during flush cycle
static bool wasteRepoLeftTriggered = false;
static bool wasteRepoRightTriggered = false;

// Relay control variables
static unsigned long relayT1StartTime = 0;
static unsigned long relayT2StartTime = 0;
static unsigned long relayP1StartTime = 0;
static unsigned long relayP2StartTime = 0;
static bool relayT1Active = false;
static bool relayT2Active = false;
static bool relayP1Active = false;
static bool relayP2Active = false;

// Relay pin definitions (from main .ino)
#define RELAY_P1_PIN 35
#define RELAY_P2_PIN 36
#define RELAY_T1_PIN 37
#define RELAY_T2_PIN 38

void activateRelay(int pin, unsigned long duration, unsigned long *startTime, bool *activeFlag)
{
  digitalWrite(pin, HIGH);
  *startTime = _currentTime;
  *activeFlag = true;
}

void updateRelays()
{
  if (relayT1Active && _currentTime - relayT1StartTime >= TOILET_FLUSH_HOLD_TIME_MS)
  {
    digitalWrite(RELAY_T1_PIN, LOW);
    relayT1Active = false;
  }
  if (relayT2Active && _currentTime - relayT2StartTime >= TOILET_FLUSH_HOLD_TIME_MS)
  {
    digitalWrite(RELAY_T2_PIN, LOW);
    relayT2Active = false;
  }
  if (relayP1Active && _currentTime - relayP1StartTime >= PUMP_RELAY_ACTIVE_TIME_MS)
  {
    digitalWrite(RELAY_P1_PIN, LOW);
    relayP1Active = false;
  }
  if (relayP2Active && _currentTime - relayP2StartTime >= PUMP_RELAY_ACTIVE_TIME_MS)
  {
    digitalWrite(RELAY_P2_PIN, LOW);
    relayP2Active = false;
  }
}

// Utility method to draw images using drawPixel
void drawImagePixels(int xPos, int yPos, int width, int height, const uint16_t *imageData, bool reflectVertically = false)
{
  for (int y = 0; y < height; y++)
  {
    for (int x = 0; x < width; x++)
    {
      uint16_t pixel = imageData[y * width + x];
      int drawX = reflectVertically ? xPos + (width - 1 - x) : xPos + x;
      tft.drawPixel(drawX, yPos + y, pixel);
    }
  }
}

void drawSaniLogo()
{
  int logoX = (SCREEN_WIDTH - LOGO_WIDTH) / 2;
  int logoY = 0;
  _saniLogoShape = new Rectangle("Sani Logo", logoX, logoY, LOGO_WIDTH, LOGO_HEIGHT);
  _saniLogoShape->printDetails();
  Serial.println("Logo position: X=" + String(logoX) + ", Y=" + String(logoY));

  for (int y = 0; y < LOGO_HEIGHT; y++)
  {
    for (int x = 0; x < LOGO_WIDTH; x++)
    {
      uint16_t pixel = sani_flush_logo_165x40[y * LOGO_WIDTH + x];
      tft.drawPixel(logoX + x, logoY + y, pixel);
    }
  }
}

void drawStartStopButton()
{
  int btnCircleRadius = 16;
  int btnCenterY = LOGO_HEIGHT / 2;
  int btnCenterX = (SCREEN_WIDTH / 2) + (LOGO_WIDTH / 2) + DEFAULT_PADDING + (btnCircleRadius * .5);

  _startStopButtonShape = new Circle("Start/Stop Button", btnCenterX, btnCenterY, btnCircleRadius * 1.5);
  _startStopButtonShape->printDetails();

  // Fill circle with black
  tft.fillCircle(btnCenterX, btnCenterY, btnCircleRadius, ILI9341_BLACK);

  int btnShapeSize = 12;
  int triangleX1 = (int16_t)(btnCenterX - btnShapeSize / 2 + 2);
  int triangleY1 = (int16_t)(btnCenterY - (int)(btnShapeSize * 0.866 / 2));
  int triangleX2 = (int16_t)(btnCenterX - btnShapeSize / 2 + 2);
  int triangleY2 = (int16_t)(btnCenterY + (int)(btnShapeSize * 0.866 / 2));
  int triangleX3 = (int16_t)(btnCenterX + btnShapeSize / 2 + 2);
  int triangleY3 = (int16_t)btnCenterY;

  int squareX = btnCenterX - btnShapeSize / 2;
  int squareY = btnCenterY - btnShapeSize / 2;

  if (_drawTriangle)
  {
    tft.fillTriangle(triangleX1, triangleY1, triangleX2, triangleY2, triangleX3, triangleY3, BTN_TRIANGLE_COLOR);
  }
  else
  {
    tft.fillRect(squareX, squareY, btnShapeSize, btnShapeSize, BTN_SQUARE_COLOR);
  }
}

void drawHamburger()
{
  int hamburgerX = DEFAULT_PADDING;
  int hamburgerY = DEFAULT_PADDING;
  int rectWidth = 70;
  int rectHeight = 5;
  int spacing = 4;
  int totalHeight = (rectHeight * 3) + (spacing * 2);

  _hamburgerShape = new Rectangle("Hamburger Menu", hamburgerX, hamburgerY, rectWidth, totalHeight);
  _hamburgerShape->printDetails();

  // Draw 3 black rectangles with 5px spacing between them
  for (int i = 0; i < 3; i++)
  {
    int rectY = hamburgerY + (i * (rectHeight + spacing));
    tft.fillRect(hamburgerX, rectY, rectWidth, rectHeight, tft.color565(16, 92, 169));
  }
}

void drawCamera(Location location)
{
  int cameraX = (location == Left) ? DEFAULT_PADDING + 18 : SCREEN_WIDTH - (DEFAULT_PADDING + 18) - CAMERA_WIDTH;
  int cameraY = LOGO_HEIGHT + DEFAULT_PADDING;

  if (location == Left)
  {
    _cameraLeftShape = new Rectangle("Left Camera", cameraX, cameraY, CAMERA_WIDTH, CAMERA_HEIGHT);
  }
  else
  {
    _cameraRightShape = new Rectangle("Right Camera", cameraX, cameraY, CAMERA_WIDTH, CAMERA_HEIGHT);
  }
  (location == Left ? _cameraLeftShape : _cameraRightShape)->printDetails();

  uint16_t cameraOutlineColor = ILI9341_DARKGREY;
  uint16_t cameraFillColor = ILI9341_BLACK;
  int cameraOutlineThickness = 3;

  tft.fillRect(cameraX, cameraY, CAMERA_WIDTH, CAMERA_HEIGHT, cameraFillColor);
  for (int i = 0; i < cameraOutlineThickness; i++)
  {
    tft.drawRect(cameraX - i, cameraY - i, CAMERA_WIDTH + (2 * i), CAMERA_HEIGHT + (2 * i), cameraOutlineColor);
  }

  int centerX = cameraX + CAMERA_WIDTH / 2;
  int centerY = cameraY + CAMERA_HEIGHT / 2;

  tft.fillCircle(centerX, centerY, 8, tft.color565(150, 150, 150));
  tft.fillCircle(centerX, centerY, 6, tft.color565(0, 30, 60));
  tft.fillCircle(centerX + 3, centerY - 3, 3, ILI9341_WHITE);
  tft.fillCircle(centerX - 5, centerY + 5, 1, tft.color565(200, 220, 255));
  tft.fillRect(cameraX + 3, cameraY + 2, 5, 3, tft.color565(255, 255, 200));
}

void drawToilet(Location location)
{
  int xPos = (location == Left) ? DEFAULT_PADDING : SCREEN_WIDTH - DEFAULT_PADDING - TOILET_WIDTH;
  int yPos = LOGO_HEIGHT + DEFAULT_PADDING + CAMERA_HEIGHT + (DEFAULT_PADDING * 2) - 3;

  if (location == Left)
  {
    _toiletLeftShape = new Rectangle("Left Toilet", xPos, yPos, TOILET_WIDTH, TOILET_HEIGHT);
    _toiletLeftShape->printDetails();
  }
  else
  {
    _toiletRightShape = new Rectangle("Right Toilet", xPos, yPos, TOILET_WIDTH, TOILET_HEIGHT);
    _toiletRightShape->printDetails();
  }

  int stage = _animStates[TOILET][location].stage;
  AnimationType *lastAnim = (location == Left) ? &lastAnimationLeft : &lastAnimationRight;

  if (_animStates[TOILET][location].active)
  {
    if (stage == 0)
    {
      drawImagePixels(xPos, yPos, TOILET_WIDTH, TOILET_HEIGHT, Toilet_Full_85x105_01);
    }
    else if (stage == 1)
    {
      drawImagePixels(xPos, yPos, TOILET_WIDTH, TOILET_HEIGHT, Toilet_Full_85x105_02);
    }
    else if (stage == 2)
    {
      drawImagePixels(xPos, yPos, TOILET_WIDTH, TOILET_HEIGHT, Toilet_Full_85x105_03);
    }
    else if (stage == 3)
    {
      drawImagePixels(xPos, yPos, TOILET_WIDTH, TOILET_HEIGHT, Toilet_Full_85x105_04);
    }
  }
  else if (*lastAnim == WASTE_REPO)
  {
    drawImagePixels(xPos, yPos, TOILET_WIDTH, TOILET_HEIGHT, Toilet_Full_85x105_05);
  }
  else
  {
    drawImagePixels(xPos, yPos, TOILET_WIDTH, TOILET_HEIGHT, Toilet_Full_85x105_04);
  }

  // Redraw timer after toilet to ensure it stays visible
  drawFlushTimer(location);
}

void drawFlushTimer(Location location)
{
  int xPos = (location == Left) ? DEFAULT_PADDING + (TOILET_WIDTH / 2) - 29 : SCREEN_WIDTH - DEFAULT_PADDING - (TOILET_WIDTH / 2) - 29;

  int yPos = LOGO_HEIGHT + DEFAULT_PADDING + CAMERA_HEIGHT + DEFAULT_PADDING + (DEFAULT_PADDING * 2) ;

  int minutes = (location == Left) ? _timerLeftMinutes : _timerRightMinutes;
  int seconds = (location == Left) ? _timerLeftSeconds : _timerRightSeconds;

  tft.setTextColor(ILI9341_BLACK, TFT_WHITE);
  tft.setTextSize(2);

  // Draw minutes block
  char minutesStr[3];
  sprintf(minutesStr, "%02d", minutes);
  tft.setCursor(xPos, yPos);
  tft.print(minutesStr);

  // Draw seconds block (with space gap)
  char secondsStr[3];
  sprintf(secondsStr, "%02d", seconds);
  tft.setCursor(xPos + 37, yPos);
  tft.print(secondsStr);
}

void drawWasteRepo(Location location)
{

  int xPos = (location == Left) ? DEFAULT_PADDING + TOILET_WIDTH : SCREEN_WIDTH - DEFAULT_PADDING - TOILET_WIDTH - WASTE_REPO_WIDTH;
  int yPos = LOGO_HEIGHT + DEFAULT_PADDING + CAMERA_HEIGHT + DEFAULT_PADDING + TOILET_HEIGHT - WASTE_REPO_HEIGHT - 8;

  if (location == Left)
  {
    _wasteRepoLeftShape = new Rectangle("Left Waste Repo", xPos, yPos, WASTE_REPO_WIDTH, WASTE_REPO_HEIGHT);
    _wasteRepoLeftShape->printDetails();
  }
  else
  {
    _wasteRepoRightShape = new Rectangle("Right Waste Repo", xPos, yPos, WASTE_REPO_WIDTH, WASTE_REPO_HEIGHT);
    _wasteRepoRightShape->printDetails();
  }

  int stage = _animStates[WASTE_REPO][location].stage;
  const uint16_t *wasteImages[] = {Waste_Repo_75x25_01, Waste_Repo_75x25_02, Waste_Repo_75x25_03, Waste_Repo_75x25_04};

  // Always use image 01 when not animating or when stage is 0
  if (!_animStates[WASTE_REPO][location].active || stage == 0)
  {
    drawImagePixels(xPos, yPos, WASTE_REPO_WIDTH, WASTE_REPO_HEIGHT, Waste_Repo_75x25_01, location == Left);
  }
  else
  {
    // During animation, use stages 1-3 (images 02-04)
    int imageIndex = (stage >= 1 && stage <= 3) ? stage : 0;
    drawImagePixels(xPos, yPos, WASTE_REPO_WIDTH, WASTE_REPO_HEIGHT, wasteImages[imageIndex], location == Left);
  }
}

void drawFlushBar()
{
  int toiletBottomY = LOGO_HEIGHT + DEFAULT_PADDING + CAMERA_HEIGHT + (DEFAULT_PADDING * 2) + TOILET_HEIGHT;
  int barHeight = 10;
  int barWidth = SCREEN_WIDTH - 10; // 5px margin each side
  int barX = 5;
  int barY = toiletBottomY + 2;
  
  // Draw black outline
  tft.drawRect(barX - 1, barY - 1, barWidth + 2, barHeight + 2, ILI9341_BLACK);
  
  // Fill with blue initially
  tft.fillRect(barX, barY, barWidth, barHeight, ILI9341_BLUE);
}

void updateFlushBar()
{
  if (!_leftFlushActive) {
    // Show empty bar when not flushing
    int toiletBottomY = LOGO_HEIGHT + DEFAULT_PADDING + CAMERA_HEIGHT + (DEFAULT_PADDING * 2) + TOILET_HEIGHT;
    int barHeight = 10;
    int barWidth = SCREEN_WIDTH - 10;
    int barX = 5;
    int barY = toiletBottomY + 2;
    
    tft.fillRect(barX, barY, barWidth, barHeight, TFT_WHITE);
    tft.drawRect(barX - 1, barY - 1, barWidth + 2, barHeight + 2, ILI9341_BLACK);
    return;
  }
  
  int toiletBottomY = LOGO_HEIGHT + DEFAULT_PADDING + CAMERA_HEIGHT + (DEFAULT_PADDING * 2) + TOILET_HEIGHT;
  int barHeight = 10;
  int barWidth = SCREEN_WIDTH - 10;
  int barX = 5;
  int barY = toiletBottomY + 2;
  
  // Calculate progress based on left flush timing
  unsigned long elapsed = _currentTime - _leftFlushStartTime;
  float progress = (float)elapsed / (float)FLUSH_DURATION_MS;
  if (progress > 1.0) progress = 1.0;
  
  // Calculate how much should be white (consumed) - from right side
  int whiteWidth = (int)(barWidth * progress);
  
  // Draw blue portion (remaining time) - from left
  int blueWidth = barWidth - whiteWidth;
  if (blueWidth > 0) {
    tft.fillRect(barX, barY, blueWidth, barHeight, ILI9341_BLUE);
  }
  
  // Draw white portion (consumed time) - from right
  if (whiteWidth > 0) {
    tft.fillRect(barX + blueWidth, barY, whiteWidth, barHeight, TFT_WHITE);
  }
  
  // Redraw outline
  tft.drawRect(barX - 1, barY - 1, barWidth + 2, barHeight + 2, ILI9341_BLACK);
}

void drawFlowDetails()
{
  // Calculate position - moved down 15px for flush bar
  int toiletBottomY = LOGO_HEIGHT + DEFAULT_PADDING + CAMERA_HEIGHT + (DEFAULT_PADDING * 2) + TOILET_HEIGHT + 15;
  int detailsHeight = SCREEN_HEIGHT - toiletBottomY;
  int xPos = 0;
  int width = SCREEN_WIDTH;

  // Draw rectangle with border
  tft.fillRect(xPos, toiletBottomY, width, detailsHeight, TFT_WHITE);
  tft.drawRect(xPos, toiletBottomY, width, detailsHeight, ILI9341_BLACK);

  // Display flow details
  tft.setTextColor(ILI9341_BLACK);
  tft.setTextSize(1);
  
  int yPos = toiletBottomY + 5;
  
  // Flush Count
  tft.setCursor(5, yPos);
  tft.print("Flush Count: " + String(_flushCount));
  yPos += 10;
  
  // Waste Consumed (using current _pumpWasteDoseML setting)
  int totalWasteOz = (_flushCount * _pumpWasteDoseML) / 30; // Convert ml to oz (1 oz ≈ 30ml)
  tft.setCursor(5, yPos);
  tft.print("Waste Consumed: " + String(totalWasteOz) + " oz");
  yPos += 10;
  
  // Gallons Flushed (2 gallons per flush)
  int totalGallons = _flushCount * 2;
  tft.setCursor(5, yPos);
  tft.print("Gallons Flushed: " + String(totalGallons));
  yPos += 10;
  
  // Current Settings
  tft.setCursor(5, yPos);
  tft.print("--- Settings ---");
  yPos += 10;
  
  // Flush Duration
  tft.setCursor(5, yPos);
  tft.print("Flush Duration: 60s");
  yPos += 10;
  
  // Waste per Flush
  tft.setCursor(5, yPos);
  tft.print("Waste/Flush: " + String(_pumpWasteDoseML) + "ml");
  yPos += 10;
  
  // Waste Pump Delay
  tft.setCursor(5, yPos);
  tft.print("Pump Delay: " + String(_wasteRepoTriggerDelayMs/1000) + "s");
}





void drawMainDisplay()
{
  Serial.println("=== DRAWING MAIN DISPLAY ===");
  tft.fillScreen(TFT_WHITE);
  Serial.println("Screen filled with white");

  Serial.println("Drawing hamburger...");
  drawHamburger();

  Serial.println("Drawing logo...");
  drawSaniLogo();

  Serial.println("Drawing start/stop button...");
  drawStartStopButton();

  Serial.println("Drawing cameras...");
  drawCamera(Left);
  drawCamera(Right);

  Serial.println("Drawing toilets...");
  drawToilet(Left);
  drawToilet(Right);

  Serial.println("Drawing waste repos...");
  drawWasteRepo(Left);
  drawWasteRepo(Right);

  Serial.println("Drawing timers...");
  drawFlushTimer(Left);
  drawFlushTimer(Right);

  Serial.println("Drawing flush bar...");
  drawFlushBar();
  
  Serial.println("Drawing flow details...");
  drawFlowDetails();

  Serial.println("=== MAIN DISPLAY COMPLETE ===");
}

void updateAnimations()
{
  // Safety check - only update animations if they should be active
  if (_flushLeft || _animStates[TOILET][Left].active)
  {
    updateToiletAnimation(Left);
  }
  if (_flushRight || _animStates[TOILET][Right].active)
  {
    updateToiletAnimation(Right);
  }
  if (_flashCameraLeft || _animStates[CAMERA][Left].active)
  {
    updateCameraFlashAnimation(Left);
  }
  if (_flashCameraRight || _animStates[CAMERA][Right].active)
  {
    updateCameraFlashAnimation(Right);
  }
  if (_animateWasteRepoLeft || _animStates[WASTE_REPO][Left].active)
  {
    updateWasteRepoAnimation(Left);
  }
  if (_animateWasteRepoRight || _animStates[WASTE_REPO][Right].active)
  {
    updateWasteRepoAnimation(Right);
  }

  // Always update timers and relays (they have their own safety checks)
  updateTimers();
  updateRelays();
  updateFlushBar();

  // Only update flush flow if it's active
  if (_flushFlowActive)
  {
    updateFlushFlow();
  }
}

void initializeFlushFlow()
{
  _flushFlowActive = true;
  _flushFlowStartTime = _currentTime;

  logStep("3. Starting left toilet flush");
  flushToilet(Left);

  logStep("Starting timers");
  _timerLeftRunning = true;
  _timerLeftStartTime = _currentTime;
  _timerLeftMinutes = 0;
  _timerLeftSeconds = 0;

  // Start right timer with delay
  _timerRightRunning = true;
  _timerRightStartTime = _currentTime + RIGHT_TOILET_FLUSH_DELAY_MS;
  _timerRightMinutes = 0;
  _timerRightSeconds = 0;
}

void flushToilet(Location location)
{
  const char *side = (location == Left) ? "Left" : "Right";
  logStep((String("Activating ") + side + " toilet flush").c_str());

  if (location == Left)
  {
    _leftFlushActive = true;
    _leftFlushStartTime = _currentTime;
    _flushLeft = true;
    activateRelay(RELAY_T1_PIN, FLUSH_DURATION_MS, &relayT1StartTime, &relayT1Active);
  }
  else
  {
    _rightFlushActive = true;
    _rightFlushStartTime = _currentTime;
    _flushRight = true;
    activateRelay(RELAY_T2_PIN, FLUSH_DURATION_MS, &relayT2StartTime, &relayT2Active);
  }
}

void updateFlushFlow()
{
  if (!_flushFlowActive)
    return;

  // Check left flush triggers
  if (_leftFlushActive)
  {
    unsigned long leftElapsed = _currentTime - _leftFlushStartTime;

    // Trigger waste repo after delay (one-time only)
    if (leftElapsed >= _wasteRepoTriggerDelayMs && !_animateWasteRepoLeft && !wasteRepoLeftTriggered)
    {
      logStep("5. Activating left waste repo");
      _animateWasteRepoLeft = true;
      wasteRepoLeftTriggered = true; // Prevent re-triggering
    }

    // End left flush and increment counter after 60 seconds
    if (leftElapsed >= FLUSH_DURATION_MS)
    {
      _leftFlushActive = false;
      wasteRepoLeftTriggered = false; // Reset for next cycle
      _flushCount++;
      logStep((String("Left flush completed - Flush count incremented to: ") + String(_flushCount)).c_str());
      updateFlushCount(0); // Update 7-digit display

      // Trigger camera only after flush completes and every 3rd flush
      if ((_flushCount % _flushCountForCameraCapture == 0) && !_flashCameraLeft)
      {
        logStep("4. Taking left camera picture");
        _flashCameraLeft = true;
        // HTTP call will happen after animation completes
      }

      // Reset left timer for next cycle
      _timerLeftStartTime = _currentTime + (_flushTotalTimeLapseMin * 60000);
      _timerLeftMinutes = 0;
      _timerLeftSeconds = 0;
      
      // Schedule left toilet restart after time lapse
      logStep("Scheduling left toilet flush restart after time lapse");
    }
  }

  // Check right flush triggers
  if (_rightFlushActive)
  {
    unsigned long rightElapsed = _currentTime - _rightFlushStartTime;

    // Trigger waste repo after delay (one-time only)
    if (rightElapsed >= _wasteRepoTriggerDelayMs && !_animateWasteRepoRight && !wasteRepoRightTriggered)
    {
      logStep("5. Activating right waste repo");
      _animateWasteRepoRight = true;
      wasteRepoRightTriggered = true; // Prevent re-triggering
    }

    // End right flush after 60 seconds (no counter increment)
    if (rightElapsed >= FLUSH_DURATION_MS)
    {
      _rightFlushActive = false;
      wasteRepoRightTriggered = false; // Reset for next cycle
      logStep("Right flush completed");

      // Trigger camera only after flush completes and every 3rd flush
      if ((_flushCount % _flushCountForCameraCapture == 0) && !_flashCameraRight)
      {
        logStep("4. Taking right camera picture");
        _flashCameraRight = true;
        // HTTP call will happen after animation completes
      }

      // Reset right timer for next cycle
      _timerRightStartTime = _currentTime + (_flushTotalTimeLapseMin * 60000) + RIGHT_TOILET_FLUSH_DELAY_MS;
      _timerRightMinutes = 0;
      _timerRightSeconds = 0;
      
      // Schedule right toilet restart after time lapse + delay
      logStep("Scheduling right toilet flush restart after time lapse + delay");
    }
  }

  // Check if it's time to start right flush (initial start)
  if (_flushFlowActive && !_rightFlushActive &&
      _currentTime - _flushFlowStartTime >= RIGHT_TOILET_FLUSH_DELAY_MS)
  {
    logStep("Starting right toilet flush after delay");
    flushToilet(Right);
  }
  
  // Check if it's time to restart left flush after time lapse
  if (_flushFlowActive && !_leftFlushActive && _timerLeftRunning &&
      _currentTime >= _timerLeftStartTime)
  {
    logStep("Restarting left toilet flush after time lapse");
    flushToilet(Left);
    _timerLeftStartTime = _currentTime; // Reset timer start
  }
  
  // Check if it's time to restart right flush after time lapse + delay
  if (_flushFlowActive && !_rightFlushActive && _timerRightRunning &&
      _currentTime >= _timerRightStartTime)
  {
    logStep("Restarting right toilet flush after time lapse + delay");
    flushToilet(Right);
    _timerRightStartTime = _currentTime; // Reset timer start
  }
}

void updateToiletAnimation(Location location)
{
  AnimationState *anim = &_animStates[TOILET][location];
  bool *flushFlag = (location == Left) ? &_flushLeft : &_flushRight;

  if (*flushFlag && !anim->active)
  {
    anim->active = true;
    anim->stage = 0;
    anim->lastTime = _currentTime;
    AnimationType *lastAnim = (location == Left) ? &lastAnimationLeft : &lastAnimationRight;
    *lastAnim = TOILET;

    // Activate toilet relay
    if (location == Left)
    {
      activateRelay(RELAY_T1_PIN, TOILET_FLUSH_HOLD_TIME_MS, &relayT1StartTime, &relayT1Active);
    }
    else
    {
      activateRelay(RELAY_T2_PIN, TOILET_FLUSH_HOLD_TIME_MS, &relayT2StartTime, &relayT2Active);
    }
  }

  if (anim->active)
  {
    if (_currentTime - anim->lastTime >= TOILET_ANIM_STAGE_DURATION_MS)
    {
      anim->stage++;
      anim->lastTime = _currentTime;

      if (anim->stage >= TOILET_ANIM_TOTAL_STAGES)
      {
        anim->stage = 0;
        anim->active = false;
        *flushFlag = false;
      }
      drawToilet(location); // Always redraw toilet (includes timer)
    }
  }
}

void updateCameraFlashAnimation(Location location)
{
  AnimationState *anim = &_animStates[CAMERA][location];
  bool *flashFlag = (location == Left) ? &_flashCameraLeft : &_flashCameraRight;

  int cameraX = (location == Left) ? DEFAULT_PADDING + 18 : SCREEN_WIDTH - (DEFAULT_PADDING + 18) - CAMERA_WIDTH;
  int cameraY = LOGO_HEIGHT + DEFAULT_PADDING;
  int centerX = cameraX + CAMERA_WIDTH / 2;
  int centerY = cameraY + CAMERA_HEIGHT / 2;
  uint16_t flashColor = tft.color565(255, 255, 0);

  if (*flashFlag && !anim->active)
  {
    anim->active = true;
    anim->stage = 1;
    anim->lastTime = _currentTime;
  }

  if (anim->active && _currentTime - anim->lastTime >= CAMERA_FLASH_STAGE_DURATION_MS)
  {
    anim->stage++;
    anim->lastTime = _currentTime;

    if (anim->stage == 2)
    {
      int rayLength = 3;
      tft.drawLine(centerX - rayLength, centerY, centerX + rayLength, centerY, flashColor);
      tft.drawLine(centerX, centerY - rayLength, centerX, centerY + rayLength, flashColor);
      tft.drawLine(centerX - rayLength / 2, centerY - rayLength / 2, centerX + rayLength / 2, centerY + rayLength / 2, flashColor);
      tft.drawLine(centerX - rayLength / 2, centerY + rayLength / 2, centerX + rayLength / 2, centerY - rayLength / 2, flashColor);
    }
    else if (anim->stage == 3)
    {
      int rayLength = 6;
      tft.drawLine(centerX - rayLength, centerY, centerX + rayLength, centerY, flashColor);
      tft.drawLine(centerX, centerY - rayLength, centerX, centerY + rayLength, flashColor);
      tft.drawLine(centerX - rayLength / 2, centerY - rayLength / 2, centerX + rayLength / 2, centerY + rayLength / 2, flashColor);
      tft.drawLine(centerX - rayLength / 2, centerY + rayLength / 2, centerX + rayLength / 2, centerY - rayLength / 2, flashColor);
    }
    else if (anim->stage == 4)
    {
      int rayLength = 10;
      tft.drawLine(centerX - rayLength, centerY, centerX + rayLength, centerY, flashColor);
      tft.drawLine(centerX, centerY - rayLength, centerX, centerY + rayLength, flashColor);
      tft.drawLine(centerX - rayLength / 2, centerY - rayLength / 2, centerX + rayLength / 2, centerY + rayLength / 2, flashColor);
      tft.drawLine(centerX - rayLength / 2, centerY + rayLength / 2, centerX + rayLength / 2, centerY - rayLength / 2, flashColor);
      tft.fillCircle(centerX, centerY, 3, flashColor);
    }
    else if (anim->stage >= CAMERA_FLASH_TOTAL_STAGES)
    {
      anim->stage = 0;
      anim->active = false;
      *flashFlag = false;
      tft.fillRect(cameraX, cameraY, CAMERA_WIDTH, CAMERA_HEIGHT, TFT_WHITE);
      drawCamera(location);

      // Make HTTP call after animation completes (for both manual and auto)
      const char *cameraID = (location == Left) ? leftCameraID : rightCameraID;
      const char *locationStr = (location == Left) ? "left_" : "right_";
      
      // Use flush count for auto captures, 0000 for manual
      String imagePrefix;
      if (_flushCount % _flushCountForCameraCapture == 0 && _flushFlowActive) {
        char flushStr[4];
        sprintf(flushStr, "%03d", _flushCount);
        imagePrefix = String(locationStr) + String(flushStr);
        Serial.println("Auto-capturing from camera (every 3 flushes)");
      } else {
        imagePrefix = String(locationStr) + "0000";
        Serial.println("Manual camera capture");
      }
      
      String serverResponse = callUploadSaniPhoto(cameraID, imagePrefix.c_str());
      Serial.println("Server response: " + serverResponse);
      
      // Update flow details after capture
      drawFlowDetails();
    }
  }
}

// Separate timing variables for each waste repo animation
static unsigned long wasteRepoLeftStartTime = 0;
static unsigned long wasteRepoRightStartTime = 0;
static bool wasteRepoLeftActive = false;
static bool wasteRepoRightActive = false;

void resetWasteRepoStates()
{
  wasteRepoLeftActive = false;
  wasteRepoRightActive = false;
  wasteRepoLeftStartTime = 0;
  wasteRepoRightStartTime = 0;
  wasteRepoCompletedLeft = false;
  wasteRepoCompletedRight = false;

  // Reset one-time trigger flags
  wasteRepoLeftTriggered = false;
  wasteRepoRightTriggered = false;

  // Reset relay states
  relayT1Active = false;
  relayT2Active = false;
  relayP1Active = false;
  relayP2Active = false;
  relayT1StartTime = 0;
  relayT2StartTime = 0;
  relayP1StartTime = 0;
  relayP2StartTime = 0;
}

void updateWasteRepoAnimation(Location location)
{
  AnimationState *anim = &_animStates[WASTE_REPO][location];
  bool *animateFlag = (location == Left) ? &_animateWasteRepoLeft : &_animateWasteRepoRight;
  unsigned long *startTime = (location == Left) ? &wasteRepoLeftStartTime : &wasteRepoRightStartTime;
  bool *activeFlag = (location == Left) ? &wasteRepoLeftActive : &wasteRepoRightActive;

  if (*animateFlag && !*activeFlag)
  {
    const char *side = (location == Left) ? "Left" : "Right";
    logStep((String(side) + " waste repo animation and relay started").c_str());
    logStep((String("PUMP_RELAY_ACTIVE_TIME_MS = ") + String(PUMP_RELAY_ACTIVE_TIME_MS)).c_str());

    *activeFlag = true;
    anim->active = true;
    anim->stage = 0;
    anim->lastTime = _currentTime;
    *startTime = _currentTime;
    AnimationType *lastAnim = (location == Left) ? &lastAnimationLeft : &lastAnimationRight;
    *lastAnim = WASTE_REPO;

    // Activate pump relay
    if (location == Left)
    {
      activateRelay(RELAY_P1_PIN, PUMP_RELAY_ACTIVE_TIME_MS, &relayP1StartTime, &relayP1Active);
    }
    else
    {
      activateRelay(RELAY_P2_PIN, PUMP_RELAY_ACTIVE_TIME_MS, &relayP2StartTime, &relayP2Active);
    }
  }

  if (*activeFlag && anim->active)
  {
    unsigned long elapsed = _currentTime - *startTime;

    // Check if animation should stop after PUMP_RELAY_ACTIVE_TIME_MS
    if (elapsed >= PUMP_RELAY_ACTIVE_TIME_MS)
    {
      const char *side = (location == Left) ? "Left" : "Right";
      String msg = String(side) + " waste repo animation and relay completed after " + String(elapsed) + "ms";
      logStep(msg.c_str());

      *activeFlag = false;
      anim->active = false;
      *animateFlag = false;
      drawToilet(location); // Update toilet to show final state (stage 5)
      return;
    }

    // Continue animation stages (5 steps: 01->02->03->04->01, 400ms each)
    if (_currentTime - anim->lastTime >= WASTE_REPO_ANIM_STAGE_DURATION_MS)
    {
      anim->stage++;
      anim->lastTime = _currentTime;
      const char *side = (location == Left) ? "Left" : "Right";
      String msg = String(side) + " waste repo stage " + String(anim->stage + 1) + "/5, elapsed: " + String(elapsed) + "ms";
      logStep(msg.c_str());

      // Stop after 5 stages (01->02->03->04->01)
      if (anim->stage >= WASTE_REPO_ANIM_TOTAL_STAGES)
      {
        String endMsg = String(side) + " waste repo animation completed - 5 stages finished";
        logStep(endMsg.c_str());
        *activeFlag = false;
        anim->active = false;
        *animateFlag = false;
        anim->stage = 0;         // Reset to stage 0 (image 01)
        drawWasteRepo(location); // Draw final default image (01)
        drawToilet(location);    // Update toilet to show final state
        return;
      }

      drawWasteRepo(location);
    }
  }
}
void toggleTimers()
{
  if (_drawTriangle) // Triangle visible = start flush flow when clicked
  {
    logStep("1. Button clicked - Starting flush workflow");
    logStep("2. Switching button from triangle to square");
    _drawTriangle = false; // Switch to square
    drawStartStopButton();

    logStep("3. Starting timer sequences");
    initializeFlushFlow();
    drawFlushBar(); // Initialize flush bar
  }
  else // Square visible = pause/stop when clicked
  {
    logStep("Button clicked - Pausing flush workflow");
    logStep("Switching button from square to triangle");
    _drawTriangle = true; // Switch to triangle
    drawStartStopButton();

    logStep("Stopping flush flow and timers");
    _flushFlowActive = false;
    _leftFlushActive = false;  // CRITICAL FIX: Prevents updateFlushFlow from re-triggering
    _rightFlushActive = false; // CRITICAL FIX: Prevents updateFlushFlow from re-triggering
    _timerLeftRunning = false;
    _timerRightRunning = false;

    // Reset animation states and stages
    _animStates[TOILET][Left].active = false;
    _animStates[TOILET][Left].stage = 0;
    _animStates[TOILET][Right].active = false;
    _animStates[TOILET][Right].stage = 0;
    _animStates[CAMERA][Left].active = false;
    _animStates[CAMERA][Left].stage = 0;
    _animStates[CAMERA][Right].active = false;
    _animStates[CAMERA][Right].stage = 0;
    _animStates[WASTE_REPO][Left].active = false;
    _animStates[WASTE_REPO][Left].stage = 0;
    _animStates[WASTE_REPO][Right].active = false;
    _animStates[WASTE_REPO][Right].stage = 0;

    // Reset flags
    _flushLeft = false;
    _flushRight = false;
    _flashCameraLeft = false;
    _flashCameraRight = false;
    _animateWasteRepoLeft = false;
    _animateWasteRepoRight = false;

    // Reset waste repo timing variables
    resetWasteRepoStates();

    // Turn off all relays immediately
    digitalWrite(RELAY_P1_PIN, LOW);
    digitalWrite(RELAY_P2_PIN, LOW);
    digitalWrite(RELAY_T1_PIN, LOW);
    digitalWrite(RELAY_T2_PIN, LOW);

    // Redraw main display to reset visual state
    drawMainDisplay();
  }
}

void updateTimers()
{
  // Update left timer
  if (_timerLeftRunning)
  {
    unsigned long elapsed = (_currentTime - _timerLeftStartTime) / 1000; // Convert to seconds
    _timerLeftMinutes = elapsed / 60;
    _timerLeftSeconds = elapsed % 60;

    // Reset at 99 minutes
    if (_timerLeftMinutes >= 99)
    {
      _timerLeftMinutes = 0;
      _timerLeftSeconds = 0;
      _timerLeftStartTime = _currentTime;
    }
  }

  // Update right timer (with delay check)
  if (_timerRightRunning)
  {
    if (_currentTime >= _timerRightStartTime) // Only start counting after delay
    {
      unsigned long elapsed = (_currentTime - _timerRightStartTime) / 1000; // Convert to seconds
      _timerRightMinutes = elapsed / 60;
      _timerRightSeconds = elapsed % 60;

      // Reset at 99 minutes
      if (_timerRightMinutes >= 99)
      {
        _timerRightMinutes = 0;
        _timerRightSeconds = 0;
        _timerRightStartTime = _currentTime;
      }
    }
    else
    {
      // Still in delay period, show 00:00
      _timerRightMinutes = 0;
      _timerRightSeconds = 0;
    }
  }

  // Redraw timers if seconds have changed
  if (_timerLeftSeconds != _lastLeftSeconds)
  {
    _lastLeftSeconds = _timerLeftSeconds;
    drawFlushTimer(Left);
  }

  if (_timerRightSeconds != _lastRightSeconds)
  {
    _lastRightSeconds = _timerRightSeconds;
    drawFlushTimer(Right);
  }
}