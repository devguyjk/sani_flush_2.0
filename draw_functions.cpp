#include "draw_functions.h"
#include "settings_system.h"
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
extern const char *left01CameraID;
extern const char *left02CameraID;
extern const char *right01CameraID;
extern const char *right02CameraID;
extern const char *uploadServerURL;
extern String left01CameraCaptureUrl;
extern String left02CameraCaptureUrl;
extern String right01CameraCaptureUrl;
extern String right02CameraCaptureUrl;
extern const unsigned long BUTTON_DEBOUNCE_MS;
extern unsigned long lastButtonPress;
extern SettingsSystem flushSettings;

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
  http.setTimeout(10000); // Reduced timeout

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

// Dual camera capture state variables
static bool pendingSecondCapture = false;
static unsigned long secondCaptureTime = 0;
static const char *pendingCameraID = nullptr;
static String pendingImagePrefix = "";
static Location pendingCaptureLocation = Left;
static bool isAutomaticCapture = false;

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
  int pumpActiveTimeMS = (flushSettings.getWasteQtyPerFlush() * 1000) / PUMP_WASTE_ML_SEC;
  if (relayP1Active && _currentTime - relayP1StartTime >= pumpActiveTimeMS)
  {
    digitalWrite(RELAY_P1_PIN, LOW);
    relayP1Active = false;
  }
  if (relayP2Active && _currentTime - relayP2StartTime >= pumpActiveTimeMS)
  {
    digitalWrite(RELAY_P2_PIN, LOW);
    relayP2Active = false;
  }
}

void captureDualCameras(Location location, bool isAuto)
{
  const char *camera01ID = (location == Left) ? left01CameraID : right01CameraID;
  const char *camera02ID = (location == Left) ? left02CameraID : right02CameraID;
  const char *locationStr = (location == Left) ? "left01" : "right01";
  const char *location2Str = (location == Left) ? "left02" : "right02";

  String imagePrefix01, imagePrefix02;
  if (isAuto)
  {
    char flushStr[4];
    sprintf(flushStr, "%03d", _flushCount);
    imagePrefix01 = String(locationStr) + "_" + String(flushStr);
    imagePrefix02 = String(location2Str) + "_" + String(flushStr);
    logStep((String("Auto-capturing from both ") + (location == Left ? "left" : "right") + " cameras (every " + String(flushSettings.getPicEveryNFlushes()) + " flushes)").c_str());
  }
  else
  {
    imagePrefix01 = String(locationStr) + "_0000";
    imagePrefix02 = String(location2Str) + "_0000";
    logStep((String("Manual capture from both ") + (location == Left ? "left" : "right") + " cameras").c_str());
  }

  // First capture immediately
  logStep((String("Capturing first image from camera ") + String(camera01ID)).c_str());
  String serverResponse01 = callUploadSaniPhoto(camera01ID, imagePrefix01.c_str());
  Serial.println("Camera 01 response: " + serverResponse01);

  // Schedule second capture with 500ms delay
  pendingSecondCapture = true;
  secondCaptureTime = _currentTime + 500;
  pendingCameraID = camera02ID;
  pendingImagePrefix = imagePrefix02;
  pendingCaptureLocation = location;
  isAutomaticCapture = isAuto;

  logStep((String("Scheduled second capture from camera ") + String(camera02ID) + " in 500ms").c_str());
}

void updatePendingCaptures()
{
  if (pendingSecondCapture && _currentTime >= secondCaptureTime)
  {
    logStep((String("Executing delayed capture from camera ") + String(pendingCameraID)).c_str());
    String serverResponse02 = callUploadSaniPhoto(pendingCameraID, pendingImagePrefix.c_str());
    Serial.println("Camera 02 response: " + serverResponse02);

    // Reset pending state
    pendingSecondCapture = false;
    pendingCameraID = nullptr;
    pendingImagePrefix = "";

    logStep("Dual camera capture sequence completed");
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

  int yPos = LOGO_HEIGHT + DEFAULT_PADDING + CAMERA_HEIGHT + DEFAULT_PADDING + (DEFAULT_PADDING * 2);

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

void drawLeftFlushBar()
{
  int barX = 10;  // Left position
  int barY = 182; // Moved up 16px
  int barWidth = 85;
  int barHeight = 10;

  // Draw black outline
  tft.drawRect(barX - 1, barY - 1, barWidth + 2, barHeight + 2, ILI9341_BLACK);
  // Fill with blue
  tft.fillRect(barX, barY, barWidth, barHeight, ILI9341_BLUE);
}

void drawRightFlushBar()
{
  int barX = 145; // Right position (240 - 10 - 85)
  int barY = 182; // Same Y as left
  int barWidth = 85;
  int barHeight = 10;

  // Draw black outline
  tft.drawRect(barX - 1, barY - 1, barWidth + 2, barHeight + 2, ILI9341_BLACK);
  // Fill with blue
  tft.fillRect(barX, barY, barWidth, barHeight, ILI9341_BLUE);
}

void updateLeftFlushBar()
{
  int barX = 10;
  int barY = 184;
  int barWidth = 85;
  int barHeight = 10;

  // Default to blue
  tft.fillRect(barX, barY, barWidth, barHeight, ILI9341_BLUE);
  // Redraw outline
  tft.drawRect(barX - 1, barY - 1, barWidth + 2, barHeight + 2, ILI9341_BLACK);
}

void updateRightFlushBar()
{
  int barX = 145;
  int barY = 184;
  int barWidth = 85;
  int barHeight = 10;

  // Default to blue
  tft.fillRect(barX, barY, barWidth, barHeight, ILI9341_BLUE);
  // Redraw outline
  tft.drawRect(barX - 1, barY - 1, barWidth + 2, barHeight + 2, ILI9341_BLACK);
}

void drawFlowDetails()
{
  // Calculate position - AFTER the flush bars
  int toiletBottomY = LOGO_HEIGHT + DEFAULT_PADDING + CAMERA_HEIGHT + (DEFAULT_PADDING * 2) + TOILET_HEIGHT;
  int barHeight = 10;
  int flowDetailsY = toiletBottomY + barHeight + 8; // Start after bars + padding
  int detailsHeight = SCREEN_HEIGHT - flowDetailsY;
  int xPos = 0;
  int width = SCREEN_WIDTH;

  // Draw rectangle with border
  tft.fillRect(xPos, flowDetailsY, width, detailsHeight, TFT_WHITE);
  tft.drawRect(xPos, flowDetailsY, width, detailsHeight, ILI9341_BLACK);

  // Display flow details
  tft.setTextColor(ILI9341_BLACK);
  tft.setTextSize(1);

  int yPos = flowDetailsY + 5;

  // Flush Count
  tft.setCursor(5, yPos);
  tft.print("Flush Count: " + String(_flushCount));
  yPos += 10;

  // Get waste per flush (already in ml)
  int wastePerFlushML = flushSettings.getWasteQtyPerFlush();

  // Waste Consumed
  int totalWasteML = _flushCount * wastePerFlushML;
  tft.setCursor(5, yPos);
  tft.print("Waste Consumed: " + String(totalWasteML) + " ml");
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

  // Flush Time Lapse
  tft.setCursor(5, yPos);
  tft.print("Flush Workflow Repeat: " + String(flushSettings.getFlushWorkflowRepeat() / 1000) + "s");
  yPos += 10;

  // Waste per Flush
  tft.setCursor(5, yPos);
  tft.print("Waste/Flush: " + String(wastePerFlushML) + "ml");
  yPos += 10;

  // Waste Pump Delay
  tft.setCursor(5, yPos);
  tft.print("Pump Delay: " + String(flushSettings.getWasteRepoTriggerDelayMs() / 1000) + "s");
  yPos += 10;

  // Camera Pic Delay
  tft.setCursor(5, yPos);
  tft.print("Cam Pic Delay: " + String(flushSettings.getCameraTriggerAfterFlushMs()) + "ms");
  yPos += 10;

  // Flushes before picture
  tft.setCursor(5, yPos);
  tft.print("Flushes b4 pic: " + String(flushSettings.getPicEveryNFlushes()));
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

  Serial.println("Drawing flush bars...");
  drawLeftFlushBar();
  drawRightFlushBar();

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
  updateLeftFlushBar();
  updateRightFlushBar();
  updatePendingCaptures();

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
  // Initialize timer to full flush duration (2 minutes)
  unsigned long flushDurationSec = flushSettings.getFlushWorkflowRepeat() / 1000;
  _timerLeftMinutes = flushDurationSec / 60;
  _timerLeftSeconds = flushDurationSec % 60;

  // Start right timer with delay
  _timerRightRunning = true;
  _timerRightStartTime = _currentTime + RIGHT_TOILET_FLUSH_DELAY_MS;
  _timerRightMinutes = flushDurationSec / 60;
  _timerRightSeconds = flushDurationSec % 60;
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
    activateRelay(RELAY_T1_PIN, flushSettings.getFlushRelayTimeLapse(), &relayT1StartTime, &relayT1Active);
  }
  else
  {
    _rightFlushActive = true;
    _rightFlushStartTime = _currentTime;
    _flushRight = true;
    activateRelay(RELAY_T2_PIN, flushSettings.getFlushRelayTimeLapse(), &relayT2StartTime, &relayT2Active);
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

    // Debug waste repo trigger conditions every 1 second
    static unsigned long lastWasteDebug = 0;
    if (_currentTime - lastWasteDebug > 1000)
    {
      Serial.println("[WASTE_DEBUG] Left elapsed: " + String(leftElapsed) + "ms, Required: " + String(flushSettings.getWasteRepoTriggerDelayMs()) + "ms");
      Serial.println("[WASTE_DEBUG] _animateWasteRepoLeft: " + String(_animateWasteRepoLeft) + ", wasteRepoLeftTriggered: " + String(wasteRepoLeftTriggered));
      lastWasteDebug = _currentTime;
    }

    // Trigger waste repo after delay (one-time only)
    if (leftElapsed >= flushSettings.getWasteRepoTriggerDelayMs() && !_animateWasteRepoLeft && !wasteRepoLeftTriggered)
    {
      logStep((String("5. Activating left waste repo after ") + String(flushSettings.getWasteRepoTriggerDelayMs()) + "ms delay").c_str());
      _animateWasteRepoLeft = true;
      wasteRepoLeftTriggered = true; // Prevent re-triggering
    }

    // End left flush after time lapse duration
    if (leftElapsed >= flushSettings.getFlushWorkflowRepeat())
    {
      _leftFlushActive = false;
      wasteRepoLeftTriggered = false; // Reset for next cycle
      _animateWasteRepoLeft = false;  // Reset waste repo flag for next cycle
      logStep("Left flush completed");

      // Reset left timer to 0 after flush completes
      _timerLeftMinutes = 0;
      _timerLeftSeconds = 0;

      // Trigger camera only after flush completes and every Nth flush (check upcoming count)
      if (((_flushCount + 1) % flushSettings.getPicEveryNFlushes() == 0) && !_flashCameraLeft)
      {
        logStep((String("4. Taking left camera picture (every ") + String(flushSettings.getPicEveryNFlushes()) + " flushes)").c_str());
        _flashCameraLeft = true;
        // HTTP call will happen after animation completes
      }

      // Schedule left toilet restart after time lapse
      _timerLeftStartTime = _currentTime;
      // Reset timer to full flush duration for next cycle
      unsigned long flushDurationSec = flushSettings.getFlushWorkflowRepeat() / 1000;
      _timerLeftMinutes = flushDurationSec / 60;
      _timerLeftSeconds = flushDurationSec % 60;

      logStep("Scheduling left toilet flush restart after time lapse");
    }
  }

  // Check right flush triggers
  if (_rightFlushActive)
  {
    unsigned long rightElapsed = _currentTime - _rightFlushStartTime;

    // Trigger waste repo after delay (one-time only)
    if (rightElapsed >= flushSettings.getWasteRepoTriggerDelayMs() && !_animateWasteRepoRight && !wasteRepoRightTriggered)
    {
      logStep((String("5. Activating right waste repo after ") + String(flushSettings.getWasteRepoTriggerDelayMs()) + "ms delay").c_str());
      _animateWasteRepoRight = true;
      wasteRepoRightTriggered = true; // Prevent re-triggering
    }

    // End right flush after time lapse duration and increment counter
    if (rightElapsed >= flushSettings.getFlushWorkflowRepeat())
    {
      _rightFlushActive = false;
      wasteRepoRightTriggered = false; // Reset for next cycle
      _animateWasteRepoRight = false;  // Reset waste repo flag for next cycle

      // Increment flush count when right toilet completes
      _flushCount++;
      logStep((String("Right flush completed - Flush count incremented to: ") + String(_flushCount)).c_str());
      updateFlushCount(1); // Increment 4-digit display by 1

      // Update flow details to reflect new flush count
      drawFlowDetails();

      // Reset right timer to 0 after flush completes
      _timerRightMinutes = 0;
      _timerRightSeconds = 0;

      // Trigger camera only after flush completes and every Nth flush - completely separate from flush count
      if ((_flushCount % flushSettings.getPicEveryNFlushes() == 0) && !_flashCameraRight)
      {
        logStep((String("4. Taking right camera picture (every ") + String(flushSettings.getPicEveryNFlushes()) + " flushes)").c_str());
        _flashCameraRight = true;
        // HTTP call will happen after animation completes
      }

      // Schedule right toilet restart after time lapse + delay
      _timerRightStartTime = _currentTime + flushSettings.getFlushWorkflowRepeat() + RIGHT_TOILET_FLUSH_DELAY_MS;
      // Reset timer to full flush duration for next cycle
      unsigned long flushDurationSec = flushSettings.getFlushWorkflowRepeat() / 1000;
      _timerRightMinutes = flushDurationSec / 60;
      _timerRightSeconds = flushDurationSec % 60;

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
  if (_flushFlowActive && !_leftFlushActive && _timerLeftRunning)
  {
    unsigned long elapsed = (_currentTime - _timerLeftStartTime) / 1000;
    if (elapsed >= (flushSettings.getFlushWorkflowRepeat() / 1000))
    {
      logStep("Restarting left toilet flush after time lapse");
      flushToilet(Left);
      // Reset timer start for new cycle
      _timerLeftStartTime = _currentTime;
    }
  }

  // Check if it's time to restart right flush after time lapse + delay
  if (_flushFlowActive && !_rightFlushActive && _timerRightRunning &&
      _currentTime >= _timerRightStartTime)
  {
    logStep("Restarting right toilet flush after time lapse + delay");
    flushToilet(Right);
    // Reset timer start for new cycle with delay
    _timerRightStartTime = _currentTime + flushSettings.getFlushWorkflowRepeat() + RIGHT_TOILET_FLUSH_DELAY_MS;
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

      // Determine if this is automatic or manual capture
      bool isAuto = (_flushCount % flushSettings.getPicEveryNFlushes() == 0 && _flushFlowActive);

      // Execute dual camera capture with delay
      captureDualCameras(location, isAuto);
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
    int pumpActiveTimeMS = (flushSettings.getWasteQtyPerFlush() * 1000) / PUMP_WASTE_ML_SEC;
    logStep((String(side) + " waste repo animation and relay started").c_str());
    logStep((String("Waste qty: ") + String(flushSettings.getWasteQtyPerFlush()) + "ml, Pump rate: " + String(PUMP_WASTE_ML_SEC) + "ml/s, Duration: " + String(pumpActiveTimeMS) + "ms").c_str());

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
      activateRelay(RELAY_P1_PIN, pumpActiveTimeMS, &relayP1StartTime, &relayP1Active);
    }
    else
    {
      activateRelay(RELAY_P2_PIN, pumpActiveTimeMS, &relayP2StartTime, &relayP2Active);
    }
  }

  if (*activeFlag && anim->active)
  {
    unsigned long elapsed = _currentTime - *startTime;
    int pumpActiveTimeMS = (flushSettings.getWasteQtyPerFlush() * 1000) / PUMP_WASTE_ML_SEC;

    // Check if animation should stop after pump active time
    if (elapsed >= pumpActiveTimeMS)
    {
      const char *side = (location == Left) ? "Left" : "Right";
      String msg = String(side) + " waste repo animation and relay completed after " + String(elapsed) + "ms";
      logStep(msg.c_str());

      // Reset all flags to allow repeated manual activation
      *activeFlag = false;
      anim->active = false;
      *animateFlag = false;
      anim->stage = 0;
      
      // Reset timing variables for this side
      *startTime = 0;
      
      drawToilet(location); // Update toilet to show final state (stage 5)
      drawWasteRepo(location); // Ensure waste repo shows default image
      
      logStep((String(side) + " waste repo ready for next activation").c_str());
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

      // Reset stage to continue cycling until pump duration completes
      if (anim->stage >= WASTE_REPO_ANIM_TOTAL_STAGES)
      {
        anim->stage = 0; // Reset to stage 0 to continue cycling
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
    drawLeftFlushBar(); // Initialize left flush bar
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
  // Update left timer - count down from flush duration
  if (_timerLeftRunning)
  {
    if (_leftFlushActive)
    {
      // During flush - count down from flush duration
      unsigned long elapsed = (_currentTime - _leftFlushStartTime) / 1000;
      unsigned long remaining = (flushSettings.getFlushWorkflowRepeat() / 1000) - elapsed;
      if (remaining > 0)
      {
        _timerLeftMinutes = remaining / 60;
        _timerLeftSeconds = remaining % 60;
      }
      else
      {
        // Reset to 0 when flush duration is complete
        _timerLeftMinutes = 0;
        _timerLeftSeconds = 0;
      }
    }
    else
    {
      // Only start countdown if we've passed the start time
      // This creates a pause showing 00:00 before starting countdown
      if (_currentTime >= _timerLeftStartTime)
      {
        // Between flushes - count down from time lapse period
        unsigned long elapsed = (_currentTime - _timerLeftStartTime) / 1000;
        unsigned long totalTime = flushSettings.getFlushWorkflowRepeat() / 1000; // Use direct seconds value
        if (elapsed < totalTime)
        {
          unsigned long remaining = totalTime - elapsed;
          _timerLeftMinutes = remaining / 60;
          _timerLeftSeconds = remaining % 60;
        }
        else
        {
          // Reset to 0 when time lapse is complete
          _timerLeftMinutes = 0;
          _timerLeftSeconds = 0;
        }
      }
      else
      {
        // Keep showing 00:00 until start time is reached
        _timerLeftMinutes = 0;
        _timerLeftSeconds = 0;
      }
    }
  }

  // Update right timer - count down from flush duration
  if (_timerRightRunning)
  {
    if (_rightFlushActive)
    {
      // During flush - count down from flush duration
      unsigned long elapsed = (_currentTime - _rightFlushStartTime) / 1000;
      unsigned long remaining = (flushSettings.getFlushWorkflowRepeat() / 1000) - elapsed;
      if (remaining > 0)
      {
        _timerRightMinutes = remaining / 60;
        _timerRightSeconds = remaining % 60;
      }
      else
      {
        // Reset to 0 when flush duration is complete
        _timerRightMinutes = 0;
        _timerRightSeconds = 0;
      }
    }
    else if (_currentTime >= _timerRightStartTime)
    {
      // Between flushes - count down to next flush
      unsigned long remaining = (_timerRightStartTime - _currentTime) / 1000;
      _timerRightMinutes = remaining / 60;
      _timerRightSeconds = remaining % 60;
    }
    else
    {
      // In delay period before first flush - show 00:00
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