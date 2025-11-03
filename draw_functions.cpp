#include "draw_functions.h"
#include "settings_system.h"
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <TJpg_Decoder.h>
#include <time.h>
#include "esp_heap_caps.h"
#include "esp_log.h"

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
extern LiquidCrystal_I2C lcd;
extern int flushCount;
extern unsigned long workflowStartTime;
extern int currentLCDState;
extern int totalWasteML;
const int LCD_RUNNING = 5;

// External references to shared counters (defined in main .ino file)
extern int leftFlushCount;
extern int rightFlushCount;
extern int imageCount;
extern int totalWasteML;
extern unsigned long workflowStartTime;

// Function declarations
void incrementWasteCounter();
void incrementLeftFlushCounter();
void incrementRightFlushCounter();
void incrementImageCounter();
void incrementWasteCounter();
void updateLCDDisplay();
void generateFlushCountString(char *buffer, size_t bufferSize);
void generateDurationString(char *buffer, size_t bufferSize);

extern const char *left01CameraID;
extern const char *left02CameraID;
extern const char *right01CameraID;
extern const char *right02CameraID;
extern const char *uploadServerURL;
String queueCameraRequest(const char *cameraID, const char *imagePrefix);

// Array of camera IDs that should flip vertically
const char* cameraIDsToFlipVert[] = {
  "5f96fe52",  // left01CameraID
  "c4df83c4"   // right01CameraID
};
const int numCamerasToFlip = sizeof(cameraIDsToFlipVert) / sizeof(cameraIDsToFlipVert[0]);
extern const unsigned long BUTTON_DEBOUNCE_MS;
extern unsigned long lastButtonPress;
extern SettingsSystem flushSettings;
extern bool _debugPrintShapeDetails;
extern HTTPClient* httpClient;
extern bool wifiNeedsRecreation;
extern unsigned long lastRightCameraCapture;
extern int completedWorkflowCycles;
extern bool firstAnalysisComplete;
extern unsigned long lastMemoryAnalysis;

// WiFi credentials - external references
extern const char *ssid;
extern const char *password;


// Logging function for workflow steps - memory optimized
void writeLog(const char *format, ...)
{
  char buffer[256]; // Fixed size buffer to prevent dynamic allocation
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  
  // Get actual local server time
  time_t now = time(0);
  struct tm *timeinfo = localtime(&now);
  
  // Format: YYMMDD.HH:MM:SS.zzz - log message
  Serial.printf("%02d%02d%02d.%02d:%02d:%02d.%03lu - %s\n", 
    (timeinfo->tm_year + 1900) % 100,  // YY (last 2 digits of year)
    timeinfo->tm_mon + 1,              // MM (month 1-12)
    timeinfo->tm_mday,                 // DD (day 1-31)
    timeinfo->tm_hour,                 // HH (hour 0-23)
    timeinfo->tm_min,                  // MM (minute 0-59)
    timeinfo->tm_sec,                  // SS (second 0-59)
    millis() % 1000,                   // zzz (milliseconds from system)
    buffer);
  
  // Don't store logs in dynamic memory - just output immediately
}

// Shared method to generate flush count string in L0000 | R0000 format
void generateFlushCountString(char *buffer, size_t bufferSize)
{
  snprintf(buffer, bufferSize, "L%04d | R%04d", leftFlushCount, rightFlushCount);
}

// Shared method to generate duration string in 000Days 15:22:15 format
void generateDurationString(char *buffer, size_t bufferSize)
{
  if (workflowStartTime > 0)
  {
    unsigned long runtime = (_currentTime - workflowStartTime) / 1000; // seconds
    int days = runtime / 86400;
    runtime %= 86400;
    int hours = runtime / 3600;
    runtime %= 3600;
    int minutes = runtime / 60;
    int seconds = runtime % 60;
    snprintf(buffer, bufferSize, "%03dDays %02d:%02d:%02d", days, hours, minutes, seconds);
  }
  else
  {
    snprintf(buffer, bufferSize, "000Days 00:00:00");
  }
}

// Shared method to calculate total gallons based on settings
float calculateTotalGallons() {
  int leftOz = flushSettings.getLeftToiletWaterOz();
  int rightOz = flushSettings.getRightToiletWaterOz();
  
  float leftGallons = (leftFlushCount * leftOz) / 128.0; // 128 oz = 1 gallon
  float rightGallons = (rightFlushCount * rightOz) / 128.0;
  
  return leftGallons + rightGallons;
}

void incrementLeftFlushCounter()
{
  leftFlushCount++;
  unsigned long timestamp = millis();
  writeLog("[COUNT] LEFT FLUSH #%d at T:%lu", leftFlushCount, timestamp);
  updateLCDDisplay();
}

void incrementRightFlushCounter()
{
  rightFlushCount++;
  unsigned long timestamp = millis();
  writeLog("[COUNT] RIGHT FLUSH #%d at T:%lu", rightFlushCount, timestamp);
  updateLCDDisplay();
}

void incrementImageCounter()
{
  imageCount++;
  writeLog("[COUNT] Image: %d", imageCount);
  updateLCDDisplay();
  drawFlowDetails();
}

void incrementWasteCounterInternal()
{
  totalWasteML += flushSettings.getWasteQtyPerFlush();
  writeLog("[COUNT] Waste: %dml (incremented by %dml)", totalWasteML, flushSettings.getWasteQtyPerFlush());
  incrementWasteCounter(); // Call the main file function to update LCD
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

// Master clock recalibration system
struct WorkflowSchedule {
  unsigned long leftNextFlushTime;
  unsigned long rightNextFlushTime;
  unsigned long absoluteWorkflowStart;
  unsigned long lastRecalibration;
};

static WorkflowSchedule schedule = {0, 0, 0, 0};
static long masterClockOffset = 0;
static const unsigned long RECALIBRATION_INTERVAL_MS = 300000; // 5 minutes
static const long DRIFT_THRESHOLD_MS = 2000; // 2 seconds

// Dual camera capture state variables
static bool pendingSecondCapture = false;
static unsigned long secondCaptureTime = 0;
static const char *pendingCameraID = nullptr;
static String pendingImagePrefix = "";
static Location pendingCaptureLocation = Left;
static bool isAutomaticCapture = false;

// Camera delay timing variables
static bool _leftCameraDelayActive = false;
static bool _rightCameraDelayActive = false;
static unsigned long _leftCameraDelayStartTime = 0;
static unsigned long _rightCameraDelayStartTime = 0;

// Relay control variables
static unsigned long relayT1StartTime = 0;
static unsigned long relayT2StartTime = 0;
static unsigned long relayP1StartTime = 0;
static unsigned long relayP2StartTime = 0;
static bool relayT1Active = false;
static bool relayT2Active = false;
static bool relayP1Active = false;
static bool relayP2Active = false;

// Relay pin definitions (from global_vars.h)
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

String queueCameraRequest(const char *cameraID, const char *imagePrefix)
{
  uint32_t memBefore = ESP.getFreeHeap();
  
  if (WiFi.status() != WL_CONNECTED)
  {
    writeLog("[QUEUE] WiFi not connected!");
    return "Error: WiFi not connected";
  }

  if (!httpClient) {
    httpClient = new HTTPClient();
    writeLog("[QUEUE] Created new HTTPClient");
  }
  
  // Use char arrays instead of String concatenation
  char url[256];
  snprintf(url, sizeof(url), "%s", uploadServerURL);
  char* lastSlash = strrchr(url, '/');
  if (lastSlash) {
    strcpy(lastSlash + 1, "queue_camera");
  }
  
  writeLog("[QUEUE] Camera: %s, URL: %s", cameraID, url);

  httpClient->begin(url);
  httpClient->addHeader("Content-Type", "application/json");
  httpClient->setTimeout(3000);

  // Check if this camera should flip vertically
  bool shouldFlip = false;
  for (int i = 0; i < numCamerasToFlip; i++) {
    if (strcmp(cameraID, cameraIDsToFlipVert[i]) == 0) {
      shouldFlip = true;
      break;
    }
  }

  // Use char array for payload
  char payload[512];
  snprintf(payload, sizeof(payload), "{\"camera_id\":\"%s\",\"image_prefix\":\"%s\",\"flip_vertical\":%s}", 
    cameraID, imagePrefix, shouldFlip ? "true" : "false");
  
  int httpResponseCode = httpClient->POST(payload);
  writeLog("[QUEUE] Response code: %d", httpResponseCode);

  char response[128] = "";
  if (httpResponseCode > 0)
  {
    // Use a fixed-size buffer to read the response stream directly
    // This avoids heap allocation from httpClient->getString()
    int len = httpClient->getSize();
    if (len > 0 && len < sizeof(response)) {
      httpClient->getStream().readBytes((uint8_t*)response, len);
      response[len] = '\0'; // Null-terminate the string
    }
  }
  else
  {
    snprintf(response, sizeof(response), "Queue failed: %d", httpResponseCode);
    writeLog("[QUEUE] Failed: %d", httpResponseCode);
  }

  writeLog("[QUEUE] Response: %s", response);

  // String objects are now tracked by ESP-IDF heap analysis
  
  uint32_t memAfter = ESP.getFreeHeap();
  writeLog("[QUEUE] Memory - Before: %d After: %d", memBefore, memAfter);
  
  return String(response);
}

void captureDualCameras(Location location, bool isAuto)
{
  uint32_t memBefore = ESP.getFreeHeap();
  writeLog("[CAMERA] Starting dual capture - Free heap: %d bytes", memBefore);
  
  const char *camera01ID = (location == Left) ? left01CameraID : right01CameraID;
  const char *camera02ID = (location == Left) ? left02CameraID : right02CameraID;
  const char *locationStr = (location == Left) ? "LFT" : "RGT";

  // Get current time for timestamp
  time_t now = time(0);
  struct tm *timeinfo = localtime(&now);

  // Use char arrays instead of String objects
  char dateStr[11]; // YYYY-MM-DD
  char timeStr[7];  // hhmmss
  char imagePrefix01[64];
  char imagePrefix02[64];
  
  sprintf(dateStr, "%04d-%02d-%02d", timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday);
  sprintf(timeStr, "%02d%02d%02d", timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);

  if (isAuto)
  {
    // Use current flush count for the respective side
    int flushNumber = (location == Left) ? leftFlushCount : rightFlushCount;
    sprintf(imagePrefix01, "%s_%s_%04d_%s_01", dateStr, timeStr, flushNumber, locationStr);
    sprintf(imagePrefix02, "%s_%s_%04d_%s_02", dateStr, timeStr, flushNumber, locationStr);
    writeLog("[CAMERA] Auto-capturing from both %s cameras (flush #%d, every %d flushes)", 
      (location == Left ? "left" : "right"), flushNumber, flushSettings.getPicEveryNFlushes());
  }
  else
  {
    sprintf(imagePrefix01, "%s_%s_0000_%s_01", dateStr, timeStr, locationStr);
    sprintf(imagePrefix02, "%s_%s_0000_%s_02", dateStr, timeStr, locationStr);
    writeLog("[CAMERA] Manual capture from both %s cameras", (location == Left ? "left" : "right"));
  }

  // Queue both cameras for capture
  writeLog("[CAMERA] Queuing dual capture from %s cameras", locationStr);
  
  writeLog("[CAMERA] Queuing camera 1/2: %s (%s)", camera01ID, locationStr);
  String queueResponse01 = queueCameraRequest(camera01ID, imagePrefix01);
  writeLog("[CAMERA] Queue 1/2 response: %s", queueResponse01.c_str());

  writeLog("[CAMERA] Queuing camera 2/2: %s (%s)", camera02ID, locationStr);
  String queueResponse02 = queueCameraRequest(camera02ID, imagePrefix02);
  writeLog("[CAMERA] Queue 2/2 response: %s", queueResponse02.c_str());

  writeLog("[CAMERA] Dual camera requests QUEUED for %s side", (location == Left) ? "LEFT" : "RIGHT");
  
  // ONLY schedule WiFi recreation after RIGHT camera operations AND not already scheduled
  if (location == Right && !wifiNeedsRecreation) {
    lastRightCameraCapture = _currentTime;
    wifiNeedsRecreation = true;
    writeLog("[CAMERA] Right camera capture completed - WiFi recreation scheduled");
  }
  else if (location == Left) {
    writeLog("[CAMERA] Left camera capture completed - no recreation needed");
  }
  
  uint32_t memAfter = ESP.getFreeHeap();
  writeLog("[CAMERA] Dual capture completed - Free heap: %d bytes (used: %d bytes)", 
    memAfter, (memBefore > memAfter) ? (memBefore - memAfter) : 0);
}

void updatePendingCaptures()
{
  if (pendingSecondCapture && _currentTime >= secondCaptureTime)
  {
    writeLog("[CAMERA] Taking picture 2/2 from camera %s (delayed)", pendingCameraID);
    String queueResponse02 = queueCameraRequest(pendingCameraID, pendingImagePrefix.c_str());
    writeLog("[CAMERA] Picture 2/2 queued - Response: %s", queueResponse02.c_str());

    // Reset pending state
    pendingSecondCapture = false;
    pendingCameraID = nullptr;
    pendingImagePrefix = "";

    writeLog("[CAMERA] Dual camera capture sequence COMPLETED for %s side", (pendingCaptureLocation == Left) ? "LEFT" : "RIGHT");
  }
}

void updateCameraDelays()
{
  // Check left camera delay - trigger dual capture immediately when delay completes
  if (_leftCameraDelayActive && _currentTime >= _leftCameraDelayStartTime)
  {
    writeLog("[CAMERA] Left camera 25s delay completed - triggering dual capture");
    _leftCameraDelayActive = false;
    
    // Trigger dual camera capture immediately (no flash animation)
    captureDualCameras(Left, true);
    incrementImageCounter();
  }

  // Check right camera delay - trigger dual capture immediately when delay completes
  if (_rightCameraDelayActive && _currentTime >= _rightCameraDelayStartTime)
  {
    writeLog("[CAMERA] Right camera 25s delay completed - triggering dual capture");
    _rightCameraDelayActive = false;
    
    // Trigger dual camera capture immediately (no flash animation)
    captureDualCameras(Right, true);
    incrementImageCounter();
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
  if (_saniLogoShape == nullptr) {
    _saniLogoShape = new Rectangle("Sani Logo", logoX, logoY, LOGO_WIDTH, LOGO_HEIGHT);
    _saniLogoShape->printDetails(_debugPrintShapeDetails);
  }
  // writeLog(("Logo position: X=" + String(logoX) + ", Y=" + String(logoY)).c_str());

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

  if (_startStopButtonShape == nullptr) {
    _startStopButtonShape = new Circle("Start/Stop Button", btnCenterX, btnCenterY, btnCircleRadius * 1.5);
    _startStopButtonShape->printDetails(_debugPrintShapeDetails);
  }

  // Fill circle with black
  tft.fillCircle(btnCenterX, btnCenterY, btnCircleRadius, TFT_BLACK);

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
  
  if (_hamburgerShape == nullptr) {
    _hamburgerShape = new Rectangle("Hamburger Menu", hamburgerX, hamburgerY, rectWidth, totalHeight);
    _hamburgerShape->printDetails(_debugPrintShapeDetails);
  }

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
  
  if (location == Left && _cameraLeftShape == nullptr) {
      _cameraLeftShape = new Rectangle("Left Camera", cameraX, cameraY, CAMERA_WIDTH, CAMERA_HEIGHT);
      _cameraLeftShape->printDetails(_debugPrintShapeDetails);
  } else if (location == Right && _cameraRightShape == nullptr) {
      _cameraRightShape = new Rectangle("Right Camera", cameraX, cameraY, CAMERA_WIDTH, CAMERA_HEIGHT);
      _cameraRightShape->printDetails(_debugPrintShapeDetails);
  }


  uint16_t cameraOutlineColor = TFT_DARKGREY;
  uint16_t cameraFillColor = TFT_BLACK;
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
  tft.fillCircle(centerX + 3, centerY - 3, 3, TFT_WHITE);
  tft.fillCircle(centerX - 5, centerY + 5, 1, tft.color565(200, 220, 255));
  tft.fillRect(cameraX + 3, cameraY + 2, 5, 3, tft.color565(255, 255, 200));
}

void drawToilet(Location location)
{
  int xPos = (location == Left) ? DEFAULT_PADDING : SCREEN_WIDTH - DEFAULT_PADDING - TOILET_WIDTH;
  int yPos = LOGO_HEIGHT + DEFAULT_PADDING + CAMERA_HEIGHT + (DEFAULT_PADDING * 2) - 3;

  if (location == Left && _toiletLeftShape == nullptr) {
      _toiletLeftShape = new Rectangle("Left Toilet", xPos, yPos, TOILET_WIDTH, TOILET_HEIGHT);
      _toiletLeftShape->printDetails(_debugPrintShapeDetails);
  } else if (location == Right && _toiletRightShape == nullptr) {
      _toiletRightShape = new Rectangle("Right Toilet", xPos, yPos, TOILET_WIDTH, TOILET_HEIGHT);
      _toiletRightShape->printDetails(_debugPrintShapeDetails);
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

  tft.setTextColor(TFT_BLACK, TFT_WHITE);
  tft.setTextSize(2);

  // Draw minutes block
  char minutesStr[3];
  sprintf(minutesStr, "%02d", minutes);
  tft.setCursor(xPos, yPos);
  tft.print(minutesStr);

  // Draw seconds block
  char secondsStr[3];
  sprintf(secondsStr, "%02d", seconds);
  tft.setCursor(xPos + 37, yPos);
  tft.print(secondsStr);
}

void drawWasteRepo(Location location)
{

  int xPos = (location == Left) ? DEFAULT_PADDING + TOILET_WIDTH : SCREEN_WIDTH - DEFAULT_PADDING - TOILET_WIDTH - WASTE_REPO_WIDTH;
  int yPos = LOGO_HEIGHT + DEFAULT_PADDING + CAMERA_HEIGHT + DEFAULT_PADDING + TOILET_HEIGHT - WASTE_REPO_HEIGHT - 8;

  if (location == Left && _wasteRepoLeftShape == nullptr) {
      _wasteRepoLeftShape = new Rectangle("Left Waste Repo", xPos, yPos, WASTE_REPO_WIDTH, WASTE_REPO_HEIGHT);
      _wasteRepoLeftShape->printDetails(_debugPrintShapeDetails);
  } else if (location == Right && _wasteRepoRightShape == nullptr) {
      _wasteRepoRightShape = new Rectangle("Right Waste Repo", xPos, yPos, WASTE_REPO_WIDTH, WASTE_REPO_HEIGHT);
      _wasteRepoRightShape->printDetails(_debugPrintShapeDetails);
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
  tft.drawRect(barX - 1, barY - 1, barWidth + 2, barHeight + 2, TFT_BLACK);
  // Fill with blue
  tft.fillRect(barX, barY, barWidth, barHeight, TFT_BLUE);
}

void drawRightFlushBar()
{
  int barX = 145; // Right position (240 - 10 - 85)
  int barY = 182; // Same Y as left
  int barWidth = 85;
  int barHeight = 10;

  // Draw black outline
  tft.drawRect(barX - 1, barY - 1, barWidth + 2, barHeight + 2, TFT_BLACK);
  // Fill with blue
  tft.fillRect(barX, barY, barWidth, barHeight, TFT_BLUE);
}

void updateLeftFlushBar()
{
  int barX = 10;
  int barY = 182;
  int barWidth = 85;
  int barHeight = 10;

  // Calculate remaining time (countdown)
  float remaining = 1.0;
  if (_leftFlushActive)
  {
    unsigned long elapsed = _currentTime - _leftFlushStartTime;
    unsigned long totalDuration = flushSettings.getFlushWorkflowRepeat();
    remaining = max(0.0f, 1.0f - (float)elapsed / (float)totalDuration);
  }
  else if (_timerLeftRunning && _currentTime >= _timerLeftStartTime)
  {
    unsigned long elapsed = _currentTime - _timerLeftStartTime;
    unsigned long totalDuration = flushSettings.getFlushWorkflowRepeat();
    remaining = max(0.0f, 1.0f - (float)elapsed / (float)totalDuration);
  }

  // Draw countdown bar (starts full, decreases to empty)
  int remainingWidth = (int)(barWidth * remaining);

  // Clear bar area
  tft.fillRect(barX, barY, barWidth, barHeight, TFT_WHITE);

  // Draw remaining time in blue
  if (remainingWidth > 0)
  {
    tft.fillRect(barX, barY, remainingWidth, barHeight, TFT_BLUE);
  }

  // Draw outline
  tft.drawRect(barX - 1, barY - 1, barWidth + 2, barHeight + 2, TFT_BLACK);
}

void updateRightFlushBar()
{
  int barX = 145;
  int barY = 182;
  int barWidth = 85;
  int barHeight = 10;

  // Calculate remaining time (countdown)
  float remaining = 1.0;
  if (_rightFlushActive)
  {
    unsigned long elapsed = _currentTime - _rightFlushStartTime;
    unsigned long totalDuration = flushSettings.getFlushWorkflowRepeat();
    remaining = max(0.0f, 1.0f - (float)elapsed / (float)totalDuration);
  }
  else if (_timerRightRunning && _currentTime < _timerRightStartTime)
  {
    // Waiting for next flush - show countdown to start
    unsigned long timeUntilStart = _timerRightStartTime - _currentTime;
    unsigned long totalDuration = flushSettings.getFlushWorkflowRepeat();
    remaining = min(1.0f, (float)timeUntilStart / (float)totalDuration);
  }
  else if (_timerRightRunning && _currentTime >= _timerRightStartTime)
  {
    // Between flushes - count down from time lapse
    unsigned long elapsed = _currentTime - _timerRightStartTime;
    unsigned long totalDuration = flushSettings.getFlushWorkflowRepeat();
    remaining = max(0.0f, 1.0f - (float)elapsed / (float)totalDuration);
  }

  // Draw countdown bar (starts full, decreases to empty)
  int remainingWidth = (int)(barWidth * remaining);

  // Clear bar area
  tft.fillRect(barX, barY, barWidth, barHeight, TFT_WHITE);

  // Draw remaining time in blue
  if (remainingWidth > 0)
  {
    tft.fillRect(barX, barY, remainingWidth, barHeight, TFT_BLUE);
  }

  // Draw outline
  tft.drawRect(barX - 1, barY - 1, barWidth + 2, barHeight + 2, TFT_BLACK);
}

void updateDuration()
{
  // Calculate same position as in drawFlowDetails
  int toiletBottomY = LOGO_HEIGHT + DEFAULT_PADDING + CAMERA_HEIGHT + (DEFAULT_PADDING * 2) + TOILET_HEIGHT;
  int barHeight = 10;
  int flowDetailsY = toiletBottomY + barHeight + 8;
  int yPos = flowDetailsY + 5;
  
  // Clear only the duration line area
  tft.fillRect(5, yPos, SCREEN_WIDTH - 10, 8, TFT_WHITE);
  
  // Redraw duration
  tft.setTextColor(TFT_BLACK);
  tft.setTextSize(1);
  char durationStr[25];
  generateDurationString(durationStr, sizeof(durationStr));
  tft.setCursor(5, yPos);
  tft.print("Duration: " + String(durationStr));
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
  tft.drawRect(xPos, flowDetailsY, width, detailsHeight, TFT_BLACK);

  // Display flow details
  tft.setTextColor(TFT_BLACK);
  tft.setTextSize(1);

  int yPos = flowDetailsY + 5;

  // Duration (runtime since workflow started)
  char durationStr[25]; // Increased for new format
  generateDurationString(durationStr, sizeof(durationStr));
  tft.setCursor(5, yPos);
  tft.print("Duration: " + String(durationStr));
  yPos += 10;

  // Flush Count in L0000 | R0000 format
  char flushCountStr[16];
  generateFlushCountString(flushCountStr, sizeof(flushCountStr));
  tft.setCursor(5, yPos);
  tft.print("Flush Count: " + String(flushCountStr));
  yPos += 10;

  // Waste Consumed - use shared totalWasteML variable (same as LCD)
  tft.setCursor(5, yPos);
  tft.print("Waste Consumed: " + String(totalWasteML) + " ml");
  yPos += 10;

  // Gallons Flushed - use same calculation as LCD (settings-based)
  float totalGallons = calculateTotalGallons();
  tft.setCursor(5, yPos);
  tft.print("Gallons Flushed: " + String((int)totalGallons));
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
  tft.print("Waste/Flush: " + String(flushSettings.getWasteQtyPerFlush()) + "ml");
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
  yPos += 10;

  // IP Address
  tft.setCursor(5, yPos);
  tft.print("IP: " + WiFi.localIP().toString());
}

void drawMainDisplay()
{
  tft.fillScreen(TFT_WHITE);

  drawHamburger();
  drawSaniLogo();
  drawStartStopButton();
  drawCamera(Left);
  drawCamera(Right);
  drawToilet(Left);
  drawToilet(Right);
  drawWasteRepo(Left);
  drawWasteRepo(Right);
  drawFlushTimer(Left);
  drawFlushTimer(Right);
  drawLeftFlushBar();
  drawRightFlushBar();
  drawFlowDetails();
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
  updatePendingCaptures();
  updateCameraDelays();

  // Update duration every second
  static unsigned long lastDurationUpdate = 0;
  if (_currentTime - lastDurationUpdate >= 1000)
  {
    updateDuration();
    lastDurationUpdate = _currentTime;
  }

  // Only update flush bars when there's actual progress to show
  static bool lastLeftFlushActive = false;
  static bool lastRightFlushActive = false;
  static unsigned long lastLeftBarUpdate = 0;
  static unsigned long lastRightBarUpdate = 0;

  // Update left bar only when state changes or every 100ms during active flush
  if (_leftFlushActive != lastLeftFlushActive ||
      (_leftFlushActive && _currentTime - lastLeftBarUpdate > 100))
  {
    updateLeftFlushBar();
    lastLeftFlushActive = _leftFlushActive;
    lastLeftBarUpdate = _currentTime;
  }

  // Update right bar only when state changes or every 100ms during active flush
  if (_rightFlushActive != lastRightFlushActive ||
      (_rightFlushActive && _currentTime - lastRightBarUpdate > 100))
  {
    updateRightFlushBar();
    lastRightFlushActive = _rightFlushActive;
    lastRightBarUpdate = _currentTime;
  }

  // Always update flush flow when workflow is active
  if (_flushFlowActive)
  {
    updateFlushFlow();
  }
  
  // Debug flush states every 30 seconds
  static unsigned long lastFlushDebug = 0;
  if (_currentTime - lastFlushDebug > 30000)
  {
    writeLog("[FLUSH_DEBUG] LF_Active:%d RF_Active:%d LF_Start:%lu RF_Start:%lu", 
      _leftFlushActive, _rightFlushActive, _leftFlushStartTime, _rightFlushStartTime);
    lastFlushDebug = _currentTime;
  }
}

unsigned long getRealTimeMillis()
{
  // Use millis() + offset for now (could be replaced with NTP/RTC)
  return millis() + masterClockOffset;
}

void recalibrateWorkflowTiming()
{
  if (!_flushFlowActive) return;
  
  unsigned long realTime = getRealTimeMillis();
  schedule.lastRecalibration = realTime;
  
  // Just validate counters without aggressive corrections
  validateFlushCounts();
  
  // Log timing status for monitoring
  unsigned long runtime = realTime - schedule.absoluteWorkflowStart;
  writeLog("[RECAL] Runtime: %lus, L:%d R:%d", runtime/1000, leftFlushCount, rightFlushCount);
}

void validateFlushCounts()
{
  if (!_flushFlowActive) return;
  
  // Only enforce the basic rule: right never exceeds left
  if (rightFlushCount > leftFlushCount)
  {
    writeLog("Correcting right count from %d to %d", rightFlushCount, leftFlushCount);
    rightFlushCount = leftFlushCount;
    updateLCDDisplay();
    drawFlowDetails();
  }
  
  // Log current state for monitoring
  writeLog("[VALIDATE] L:%d R:%d", leftFlushCount, rightFlushCount);
}

void initializeFlushFlow()
{
  _flushFlowActive = true;
  _flushFlowStartTime = _currentTime;
  workflowStartTime = _currentTime;
  _initialRightFlushStarted = false;
  
  unsigned long flushDurationSec = flushSettings.getFlushWorkflowRepeat() / 1000;
  unsigned long rightFlushDelayMs = flushSettings.getRightToiletFlushDelaySec() * 1000;
  
  writeLog("[INIT] Workflow started - duration:%lus", flushDurationSec);

  // Start left timer and flush immediately
  _timerLeftRunning = true;
  _timerLeftStartTime = _currentTime;
  _timerLeftMinutes = flushDurationSec / 60;
  _timerLeftSeconds = flushDurationSec % 60;
  incrementLeftFlushCounter();
  flushToilet(Left);

  // Start right timer with delay
  _timerRightRunning = true;
  _timerRightStartTime = _currentTime + rightFlushDelayMs;
  _timerRightMinutes = flushDurationSec / 60;
  _timerRightSeconds = flushDurationSec % 60;
  
  writeLog("[INIT] Timers started - Left:immediate Right:%lus delay", rightFlushDelayMs/1000);
}

void flushToilet(Location location)
{
  const char *side = (location == Left) ? "Left" : "Right";
  writeLog("Activating %s toilet flush", side);

  if (location == Left)
  {
    _leftFlushActive = true;
    _leftFlushStartTime = _currentTime;
    _flushLeft = true;
    wasteRepoLeftTriggered = false; // Reset waste repo trigger flag
    activateRelay(RELAY_T1_PIN, flushSettings.getFlushRelayTimeLapse(), &relayT1StartTime, &relayT1Active);
    writeLog("[DEBUG] Left flush active - start time: %lu", _leftFlushStartTime);
  }
  else
  {
    _rightFlushActive = true;
    _rightFlushStartTime = _currentTime;
    _flushRight = true;
    wasteRepoRightTriggered = false; // Reset waste repo trigger flag
    activateRelay(RELAY_T2_PIN, flushSettings.getFlushRelayTimeLapse(), &relayT2StartTime, &relayT2Active);
    writeLog("[DEBUG] Right flush active - start time: %lu", _rightFlushStartTime);
  }
}

void updateFlushFlow()
{
  if (!_flushFlowActive)
    return;

  // Handle active flush cycles - manage waste repo and camera triggers
  if (_leftFlushActive)
  {
    unsigned long leftElapsed = _currentTime - _leftFlushStartTime;

    // Trigger waste repo after 7-second delay (one-time only)
    unsigned long wasteDelay = flushSettings.getWasteRepoTriggerDelayMs();
    if (leftElapsed >= wasteDelay && !_animateWasteRepoLeft && !wasteRepoLeftTriggered)
    {
      writeLog("[WASTE] Left waste repo triggered after %lums delay", wasteDelay);
      _animateWasteRepoLeft = true;
      wasteRepoLeftTriggered = true;
    }

    // End left flush after duration
    if (leftElapsed >= flushSettings.getFlushWorkflowRepeat())
    {
      _leftFlushActive = false;
      wasteRepoLeftTriggered = false;
      _animateWasteRepoLeft = false;
      writeLog("[FLUSH] Left flush completed after %lums", leftElapsed);

      // Trigger camera if needed
      if ((leftFlushCount % flushSettings.getPicEveryNFlushes() == 0) && !_flashCameraLeft)
      {
        writeLog("[CAMERA] Scheduling left camera (flush #%d, every %d flushes)", leftFlushCount, flushSettings.getPicEveryNFlushes());
        _leftCameraDelayStartTime = _currentTime;
        _leftCameraDelayActive = true;
      }
      else
      {
        writeLog("[CAMERA] Left camera NOT triggered - flush #%d, modulo=%d, every=%d", leftFlushCount, leftFlushCount % flushSettings.getPicEveryNFlushes(), flushSettings.getPicEveryNFlushes());
      }
    }
  }

  if (_rightFlushActive)
  {
    unsigned long rightElapsed = _currentTime - _rightFlushStartTime;

    // Trigger waste repo after 7-second delay (one-time only)
    unsigned long wasteDelay = flushSettings.getWasteRepoTriggerDelayMs();
    if (rightElapsed >= wasteDelay && !_animateWasteRepoRight && !wasteRepoRightTriggered)
    {
      writeLog("[WASTE] Right waste repo triggered after %lums delay", wasteDelay);
      _animateWasteRepoRight = true;
      wasteRepoRightTriggered = true;
    }

    // End right flush after duration
    if (rightElapsed >= flushSettings.getFlushWorkflowRepeat())
    {
      _rightFlushActive = false;
      wasteRepoRightTriggered = false;
      _animateWasteRepoRight = false;
      writeLog("[FLUSH] Right flush completed after %lums", rightElapsed);

      // Trigger camera if needed
      if ((rightFlushCount % flushSettings.getPicEveryNFlushes() == 0) && !_flashCameraRight)
      {
        writeLog("[CAMERA] Scheduling right camera (flush #%d, every %d flushes)", rightFlushCount, flushSettings.getPicEveryNFlushes());
        _rightCameraDelayStartTime = _currentTime;
        _rightCameraDelayActive = true;
      }
      else
      {
        writeLog("[CAMERA] Right camera NOT triggered - flush #%d, modulo=%d, every=%d", rightFlushCount, rightFlushCount % flushSettings.getPicEveryNFlushes(), flushSettings.getPicEveryNFlushes());
      }
    }
  }

  // Handle initial right toilet start (only once)
  unsigned long rightFlushDelayMs = flushSettings.getRightToiletFlushDelaySec() * 1000;
  if (!_rightFlushActive && !_initialRightFlushStarted &&
      _currentTime - _flushFlowStartTime >= rightFlushDelayMs)
  {
    writeLog("Starting initial right toilet flush after %lus delay", rightFlushDelayMs/1000);
    incrementRightFlushCounter();
    flushToilet(Right);
    _initialRightFlushStarted = true;
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
      bool isAuto = _flushFlowActive;
      writeLog("[CAMERA] Flash animation completed for %s side - triggering dual capture", (location == Left) ? "left" : "right");

      // Execute dual camera capture with delay
      captureDualCameras(location, isAuto);
      incrementImageCounter(); // Increment image counter
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

// Simplified memory snapshot for comparison
void MemorySnapshot::capture() {
  freeHeap = esp_get_free_heap_size();
  minFreeHeap = esp_get_minimum_free_heap_size();
  trackedObjects = 0; // Simplified - ESP-IDF handles tracking
  timestamp = millis();
}

void MemorySnapshot::compare(MemorySnapshot& previous) {
  writeLog("[MEM_COMPARE] === MEMORY CHANGE ANALYSIS ===");
  writeLog("[MEM_COMPARE] Free heap: %d -> %d (change: %d)", 
           previous.freeHeap, freeHeap, (int)(freeHeap - previous.freeHeap));
  writeLog("[MEM_COMPARE] Min free: %d -> %d (change: %d)", 
           previous.minFreeHeap, minFreeHeap, (int)(minFreeHeap - previous.minFreeHeap));
  writeLog("[MEM_COMPARE] Time elapsed: %lu ms", timestamp - previous.timestamp);
}

void logMemoryObjects() {
  writeLog("[MEM_ANALYSIS] === ESP-IDF HEAP ANALYSIS ===");
  
  // Basic heap info
  size_t free_heap = esp_get_free_heap_size();
  size_t min_free_heap = esp_get_minimum_free_heap_size();
  writeLog("[MEM_ANALYSIS] Free Heap: %u bytes, Min Free: %u bytes", free_heap, min_free_heap);
  
  // Detailed heap breakdown by capability
  const uint32_t caps_to_check[] = {
    MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT,  // Internal RAM
    MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT,    // External PSRAM
    MALLOC_CAP_DMA,                         // DMA-capable memory
  };
  
  const char* cap_names[] = {
    "Internal RAM",
    "PSRAM", 
    "DMA Memory"
  };
  
  for (int i = 0; i < 3; i++) {
    multi_heap_info_t info;
    heap_caps_get_info(&info, caps_to_check[i]);
    
    writeLog("[MEM_ANALYSIS] %s - Free: %u, Largest: %u, Allocated: %u", 
             cap_names[i], 
             info.total_free_bytes, 
             info.largest_free_block,
             info.total_allocated_bytes);
  }
  
  writeLog("[MEM_ANALYSIS] === END ANALYSIS ===");
}

void checkMemoryAnalysisTrigger() {
  bool shouldAnalyze = false;
  
  if (!firstAnalysisComplete && imageCount >= 2) {
    shouldAnalyze = true;
    firstAnalysisComplete = true;
    writeLog("[MEM_TRIGGER] First workflow complete - triggering analysis");
  }
  else if (firstAnalysisComplete && (completedWorkflowCycles % 3 == 0) && 
           _currentTime - lastMemoryAnalysis > 300000) {
    shouldAnalyze = true;
    writeLog("[MEM_TRIGGER] Cycle %d complete - triggering analysis", completedWorkflowCycles);
  }
  
  if (shouldAnalyze) {
    logMemoryObjects();
    lastMemoryAnalysis = _currentTime;
  }
}

HTTPClient* createTrackedHTTPClient() {
  size_t beforeHeap = esp_get_free_heap_size();
  HTTPClient* client = new HTTPClient();
  size_t afterHeap = esp_get_free_heap_size();
  
  writeLog("[HTTP_TRACK] HTTPClient created - heap change: %d bytes", (int)(beforeHeap - afterHeap));
  return client;
}

void destroyTrackedHTTPClient(HTTPClient* client) {
  if (!client) return;
  
  size_t beforeHeap = esp_get_free_heap_size();
  client->end();
  delete client;
  size_t afterHeap = esp_get_free_heap_size();
  
  writeLog("[HTTP_TRACK] HTTPClient destroyed - heap change: %d bytes", (int)(afterHeap - beforeHeap));
}

void recreateNetworkObjects()
{
  size_t beforeHeap = esp_get_free_heap_size();
  writeLog("[WIFI_RESET] Starting WiFi/HTTP recreation - Free: %u", beforeHeap);
  
  // Destroy existing HTTP client with tracking
  if (httpClient) {
    destroyTrackedHTTPClient(httpClient);
    httpClient = nullptr;
  }
  
  // Disconnect and reconnect WiFi
  WiFi.disconnect(true);
  delay(100);
  writeLog("[WIFI_RESET] WiFi disconnected");
  
  WiFi.begin(ssid, password);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    writeLog("[WIFI_RESET] WiFi reconnected - IP: %s", WiFi.localIP().toString().c_str());
  } else {
    writeLog("[WIFI_RESET] WiFi reconnection failed");
  }
  
  // Create new HTTP client with tracking
  httpClient = createTrackedHTTPClient();
  
  wifiNeedsRecreation = false;
  size_t afterHeap = esp_get_free_heap_size();
  writeLog("[WIFI_RESET] Completed - Free: %u (recovered: %d bytes)", afterHeap, (int)(afterHeap - beforeHeap));
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
    writeLog("%s waste repo animation and relay started", side);
    writeLog("Waste qty: %dml, Pump rate: %dml/s, Duration: %dms", 
      flushSettings.getWasteQtyPerFlush(), PUMP_WASTE_ML_SEC, pumpActiveTimeMS);

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
    // Note: Waste counter incremented when animation completes
  }

  if (*activeFlag && anim->active)
  {
    unsigned long elapsed = _currentTime - *startTime;
    int pumpActiveTimeMS = (flushSettings.getWasteQtyPerFlush() * 1000) / PUMP_WASTE_ML_SEC;

    // Check if animation should stop after pump active time
    if (elapsed >= pumpActiveTimeMS)
    {
      const char *side = (location == Left) ? "Left" : "Right";
      writeLog("%s waste repo animation and relay completed after %lums", side, elapsed);

      // Reset all flags to allow repeated manual activation
      *activeFlag = false;
      anim->active = false;
      *animateFlag = false;
      anim->stage = 0;

      // Reset timing variables for this side
      *startTime = 0;
      
      // Increment waste counter when animation completes
      totalWasteML += flushSettings.getWasteQtyPerFlush();
      writeLog("[COUNT] Waste: %dml (incremented by %dml)", totalWasteML, flushSettings.getWasteQtyPerFlush());
      
      // Update both LCD and TFT displays
      updateLCDDisplay();
      drawFlowDetails();

      drawToilet(location);    // Update toilet to show final state (stage 5)
      drawWasteRepo(location); // Ensure waste repo shows default image

      writeLog("%s waste repo ready for next activation", side);
      return;
    }

    // Continue animation stages (5 steps: 01->02->03->04->01, 400ms each)
    if (_currentTime - anim->lastTime >= WASTE_REPO_ANIM_STAGE_DURATION_MS)
    {
      anim->stage++;
      anim->lastTime = _currentTime;
      const char *side = (location == Left) ? "Left" : "Right";
      // writeLog("%s waste repo stage %d/5, elapsed: %lums", side, anim->stage + 1, elapsed); // Too verbose

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
    unsigned long timestamp = millis();
    writeLog("[BUTTON] START clicked at T:%lu", timestamp);
    _drawTriangle = false; // Switch to square
    drawStartStopButton();

    initializeFlushFlow();
    drawLeftFlushBar(); // Initialize left flush bar
    updateLCDDisplay(); // Update LCD to show running state
  }
  else // Square visible = pause/stop when clicked
  {
    writeLog("Button clicked - Pausing flush workflow");
    writeLog("Switching button from square to triangle");
    _drawTriangle = true; // Switch to triangle
    drawStartStopButton();

    writeLog("Stopping flush flow and timers");
    _flushFlowActive = false;
    _leftFlushActive = false;  // CRITICAL FIX: Prevents updateFlushFlow from re-triggering
    _rightFlushActive = false; // CRITICAL FIX: Prevents updateFlushFlow from re-triggering
    _initialRightFlushStarted = false; // Reset the new flag
    _timerLeftRunning = false;
    _timerRightRunning = false;

    // Reset counters display
    updateLCDDisplay(); // Update LCD to show stopped state

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

    // Reset camera delay states
    _leftCameraDelayActive = false;
    _rightCameraDelayActive = false;
    _leftCameraDelayStartTime = 0;
    _rightCameraDelayStartTime = 0;

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
  static unsigned long lastDebugLog = 0;
  
  // Track completed workflow cycles
  static int lastLeftCount = 0;
  static int lastRightCount = 0;
  
  if (leftFlushCount > lastLeftCount && rightFlushCount > lastRightCount) {
    completedWorkflowCycles++;
    lastLeftCount = leftFlushCount;
    lastRightCount = rightFlushCount;
    writeLog("[WORKFLOW] Completed cycle %d (L:%d R:%d)", completedWorkflowCycles, leftFlushCount, rightFlushCount);
  }
  
  // Update left timer - unified timer-driven system
  if (_timerLeftRunning)
  {
    unsigned long elapsed = (_currentTime - _timerLeftStartTime) / 1000;
    unsigned long totalTimeSeconds = flushSettings.getFlushWorkflowRepeat() / 1000;
    
    if (elapsed < totalTimeSeconds)
    {
      unsigned long remaining = totalTimeSeconds - elapsed;
      _timerLeftMinutes = remaining / 60;
      _timerLeftSeconds = remaining % 60;
      
      // Debug log every 10 seconds
      if (_currentTime - lastDebugLog >= 10000)
      {
        writeLog("[TIMER] Left: %02d:%02d (elapsed:%lus)", _timerLeftMinutes, _timerLeftSeconds, elapsed);
        lastDebugLog = _currentTime;
      }
    }
    else
    {
      // Timer reached 00:00 - trigger new cycle immediately
      writeLog("[CYCLE] Left timer reached 00:00 - starting new cycle");
      
      // 1. Check if previous flush should trigger camera
      if ((leftFlushCount % flushSettings.getPicEveryNFlushes() == 0))
      {
        writeLog("[CAMERA] Triggering left camera after 25s delay (flush #%d, every %d flushes)", leftFlushCount, flushSettings.getPicEveryNFlushes());
        
        // Schedule dual camera capture after 25-second delay
        pendingSecondCapture = false; // Reset any pending capture
        unsigned long cameraDelay = flushSettings.getCameraTriggerAfterFlushMs();
        
        // Schedule first camera immediately after delay
        _leftCameraDelayStartTime = _currentTime + cameraDelay;
        _leftCameraDelayActive = true;
        
        writeLog("[CAMERA] Left dual capture scheduled in %lums", cameraDelay);
      }
      else
      {
        writeLog("[CAMERA] Left camera NOT triggered - flush #%d, modulo=%d, every=%d", leftFlushCount, leftFlushCount % flushSettings.getPicEveryNFlushes(), flushSettings.getPicEveryNFlushes());
      }
      
      // 2. Reset timer to full duration
      _timerLeftStartTime = _currentTime;
      _timerLeftMinutes = totalTimeSeconds / 60;
      _timerLeftSeconds = totalTimeSeconds % 60;
      
      // 3. Start new flush cycle
      incrementLeftFlushCounter();
      flushToilet(Left);
    }
  }

  // Update right timer - unified timer-driven system
  if (_timerRightRunning)
  {
    // Handle initial delay for right toilet
    if (_currentTime < _timerRightStartTime)
    {
      _timerRightMinutes = 0;
      _timerRightSeconds = 0;
    }
    else
    {
      unsigned long elapsed = (_currentTime - _timerRightStartTime) / 1000;
      unsigned long totalTimeSeconds = flushSettings.getFlushWorkflowRepeat() / 1000;
      
      if (elapsed < totalTimeSeconds)
      {
        unsigned long remaining = totalTimeSeconds - elapsed;
        _timerRightMinutes = remaining / 60;
        _timerRightSeconds = remaining % 60;
      }
      else
      {
        // Timer reached 00:00 - trigger new cycle immediately
        writeLog("[CYCLE] Right timer reached 00:00 - starting new cycle");
        
        // 1. Check if previous flush should trigger camera
        if ((rightFlushCount % flushSettings.getPicEveryNFlushes() == 0))
        {
          writeLog("[CAMERA] Triggering right camera after 25s delay (flush #%d, every %d flushes)", rightFlushCount, flushSettings.getPicEveryNFlushes());
          
          // Schedule dual camera capture after 25-second delay
          pendingSecondCapture = false; // Reset any pending capture
          unsigned long cameraDelay = flushSettings.getCameraTriggerAfterFlushMs();
          
          // Schedule first camera immediately after delay
          _rightCameraDelayStartTime = _currentTime + cameraDelay;
          _rightCameraDelayActive = true;
          
          writeLog("[CAMERA] Right dual capture scheduled in %lums", cameraDelay);
        }
        else
        {
          writeLog("[CAMERA] Right camera NOT triggered - flush #%d, modulo=%d, every=%d", rightFlushCount, rightFlushCount % flushSettings.getPicEveryNFlushes(), flushSettings.getPicEveryNFlushes());
        }
        
        // 2. Reset timer to full duration
        _timerRightStartTime = _currentTime;
        _timerRightMinutes = totalTimeSeconds / 60;
        _timerRightSeconds = totalTimeSeconds % 60;
        
        // 3. Start new flush cycle
        incrementRightFlushCounter();
        flushToilet(Right);
      }
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