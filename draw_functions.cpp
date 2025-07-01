#include "draw_functions.h"
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
enum AnimationType { TOILET = 0, CAMERA = 1, WASTE_REPO = 2 };

// Static variables to track timer display updates
static unsigned long _lastTimerUpdate = 0;
static int _lastLeftSeconds = -1;
static int _lastRightSeconds = -1;
static bool wasteRepoCompletedLeft = false;
static bool wasteRepoCompletedRight = false;
static AnimationType lastAnimationLeft = TOILET;
static AnimationType lastAnimationRight = TOILET;

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

void activateRelay(int pin, unsigned long duration, unsigned long* startTime, bool* activeFlag)
{
  digitalWrite(pin, HIGH);
  *startTime = _currentTime;
  *activeFlag = true;
}

void updateRelays()
{
  if (relayT1Active && _currentTime - relayT1StartTime >= TOILET_FLUSH_HOLD_TIME_MS) {
    digitalWrite(RELAY_T1_PIN, LOW);
    relayT1Active = false;
  }
  if (relayT2Active && _currentTime - relayT2StartTime >= TOILET_FLUSH_HOLD_TIME_MS) {
    digitalWrite(RELAY_T2_PIN, LOW);
    relayT2Active = false;
  }
  if (relayP1Active && _currentTime - relayP1StartTime >= PUMP_RELAY_ACTIVE_TIME_MS) {
    digitalWrite(RELAY_P1_PIN, LOW);
    relayP1Active = false;
  }
  if (relayP2Active && _currentTime - relayP2StartTime >= PUMP_RELAY_ACTIVE_TIME_MS) {
    digitalWrite(RELAY_P2_PIN, LOW);
    relayP2Active = false;
  }
}

// Utility method to draw images using drawPixel
void drawImagePixels(int xPos, int yPos, int width, int height, const uint16_t* imageData, bool reflectVertically = false)
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
  int logoXPixel = SCREEN_WIDTH - logoX;
  int logoYPixel = SCREEN_HEIGHT - logoY;
  _saniLogoShape = new Rectangle("Sani Logo", logoXPixel, logoYPixel, LOGO_WIDTH, LOGO_HEIGHT);
  _saniLogoShape->printDetails();

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
    tft.fillRect(hamburgerX, rectY, rectWidth, rectHeight, tft.color565(16,92,169));
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
  int yPos = LOGO_HEIGHT + DEFAULT_PADDING + CAMERA_HEIGHT + (DEFAULT_PADDING * 2);

  if (location == Left) {
    _toiletLeftShape = new Rectangle("Left Toilet", xPos, yPos, TOILET_WIDTH, TOILET_HEIGHT);
    _toiletLeftShape->printDetails();
  } else {
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
    } else if (stage == 1)
    {
      drawImagePixels(xPos, yPos, TOILET_WIDTH, TOILET_HEIGHT, Toilet_Full_85x105_02);
    } else if (stage == 2)
    {
      drawImagePixels(xPos, yPos, TOILET_WIDTH, TOILET_HEIGHT, Toilet_Full_85x105_03);
    } else if (stage == 3)
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
}

void drawFlushTimer(Location location)
{
  int xPos = (location == Left) ? DEFAULT_PADDING + (TOILET_WIDTH / 2) - 30 : 
                 SCREEN_WIDTH - DEFAULT_PADDING - (TOILET_WIDTH / 2) - 30;

  int yPos = LOGO_HEIGHT + DEFAULT_PADDING + CAMERA_HEIGHT + DEFAULT_PADDING + TOILET_HEIGHT + (DEFAULT_PADDING * 2);

  int minutes = (location == Left) ? _timerLeftMinutes : _timerRightMinutes;
  int seconds = (location == Left) ? _timerLeftSeconds : _timerRightSeconds;

  char timerStr[6];
  sprintf(timerStr, "%02d:%02d", minutes, seconds);

  tft.fillRect(xPos - 10, yPos, 80, 25, TFT_WHITE);

  tft.setTextColor(ILI9341_BLACK);
  tft.setTextSize(2);
  tft.setCursor(xPos, yPos);
  tft.print(timerStr);
}

void drawWasteRepo(Location location)
{
  
  int xPos = (location == Left) ? DEFAULT_PADDING + TOILET_WIDTH : 
  SCREEN_WIDTH - DEFAULT_PADDING - TOILET_WIDTH - WASTE_REPO_WIDTH;
  int yPos = LOGO_HEIGHT + DEFAULT_PADDING + CAMERA_HEIGHT + DEFAULT_PADDING + TOILET_HEIGHT - WASTE_REPO_HEIGHT - 8;

  if (location == Left) {
    _wasteRepoLeftShape = new Rectangle("Left Waste Repo", xPos, yPos, WASTE_REPO_WIDTH, WASTE_REPO_HEIGHT);
    _wasteRepoLeftShape->printDetails();
  } else {
    _wasteRepoRightShape = new Rectangle("Right Waste Repo", xPos, yPos, WASTE_REPO_WIDTH, WASTE_REPO_HEIGHT);
    _wasteRepoRightShape->printDetails();
  }

  int stage = _animStates[WASTE_REPO][location].stage % 4;
  const uint16_t* wasteImages[] = {Waste_Repo_75x25_01, Waste_Repo_75x25_02, Waste_Repo_75x25_03, Waste_Repo_75x25_04};

  drawImagePixels(xPos, yPos, WASTE_REPO_WIDTH, WASTE_REPO_HEIGHT, wasteImages[stage], location == Left);
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
}

void updateAnimations()
{
  updateToiletAnimation(Left);
  updateToiletAnimation(Right);
  updateCameraFlashAnimation(Left);
  updateCameraFlashAnimation(Right);
  updateWasteRepoAnimation(Left);
  updateWasteRepoAnimation(Right);
  updateTimers();
  updateRelays();
  updateFlushFlow();
}

void initializeFlushFlow()
{
  _flushFlowActive = true;
  _flushFlowStartTime = _currentTime;
  
  // Start left flush immediately
  flushToilet(Left);
  
  // Start timers
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
  _flushCount++;
  
  if (location == Left) {
    _leftFlushActive = true;
    _leftFlushStartTime = _currentTime;
    _flushLeft = true;
    activateRelay(RELAY_T1_PIN, _flushTotalDurationMs, &relayT1StartTime, &relayT1Active);
  } else {
    _rightFlushActive = true;
    _rightFlushStartTime = _currentTime;
    _flushRight = true;
    activateRelay(RELAY_T2_PIN, _flushTotalDurationMs, &relayT2StartTime, &relayT2Active);
  }
}

void updateFlushFlow()
{
  if (!_flushFlowActive) return;
  
  // Check left flush triggers
  if (_leftFlushActive) {
    unsigned long leftElapsed = _currentTime - _leftFlushStartTime;
    
    // Trigger waste repo after delay
    if (leftElapsed >= _wasteRepoTriggerDelayMs && !_animateWasteRepoLeft) {
      _animateWasteRepoLeft = true;
    }
    
    // Trigger camera after flush starts (when flush count matches)
    if (leftElapsed >= _cameraTriggerAfterFlushMs && 
        (_flushCount % _flushCountForCamera == 0) && !_flashCameraLeft) {
      _flashCameraLeft = true;
    }
    
    // End left flush
    if (leftElapsed >= _flushTotalDurationMs) {
      _leftFlushActive = false;
    }
  }
  
  // Check right flush triggers
  if (_rightFlushActive) {
    unsigned long rightElapsed = _currentTime - _rightFlushStartTime;
    
    // Trigger waste repo after delay
    if (rightElapsed >= _wasteRepoTriggerDelayMs && !_animateWasteRepoRight) {
      _animateWasteRepoRight = true;
    }
    
    // Trigger camera after flush starts (when flush count matches)
    if (rightElapsed >= _cameraTriggerAfterFlushMs && 
        (_flushCount % _flushCountForCamera == 0) && !_flashCameraRight) {
      _flashCameraRight = true;
    }
    
    // End right flush
    if (rightElapsed >= _flushTotalDurationMs) {
      _rightFlushActive = false;
    }
  }
  
  // Check if it's time to start right flush
  if (_flushFlowActive && !_rightFlushActive && !_leftFlushActive && 
      _currentTime - _flushFlowStartTime >= RIGHT_TOILET_FLUSH_DELAY_MS) {
    flushToilet(Right);
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
    if (location == Left) {
      activateRelay(RELAY_T1_PIN, TOILET_FLUSH_HOLD_TIME_MS, &relayT1StartTime, &relayT1Active);
    } else {
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
        drawToilet(location); // Draw final state after animation
      }
      else
      {
        drawToilet(location);
      }
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
    
    if (anim->stage == 2) {
      int rayLength = 3;
      tft.drawLine(centerX - rayLength, centerY, centerX + rayLength, centerY, flashColor);
      tft.drawLine(centerX, centerY - rayLength, centerX, centerY + rayLength, flashColor);
      tft.drawLine(centerX - rayLength/2, centerY - rayLength/2, centerX + rayLength/2, centerY + rayLength/2, flashColor);
      tft.drawLine(centerX - rayLength/2, centerY + rayLength/2, centerX + rayLength/2, centerY - rayLength/2, flashColor);
    } else if (anim->stage == 3) {
      int rayLength = 6;
      tft.drawLine(centerX - rayLength, centerY, centerX + rayLength, centerY, flashColor);
      tft.drawLine(centerX, centerY - rayLength, centerX, centerY + rayLength, flashColor);
      tft.drawLine(centerX - rayLength/2, centerY - rayLength/2, centerX + rayLength/2, centerY + rayLength/2, flashColor);
      tft.drawLine(centerX - rayLength/2, centerY + rayLength/2, centerX + rayLength/2, centerY - rayLength/2, flashColor);
    } else if (anim->stage == 4) {
      int rayLength = 10;
      tft.drawLine(centerX - rayLength, centerY, centerX + rayLength, centerY, flashColor);
      tft.drawLine(centerX, centerY - rayLength, centerX, centerY + rayLength, flashColor);
      tft.drawLine(centerX - rayLength/2, centerY - rayLength/2, centerX + rayLength/2, centerY + rayLength/2, flashColor);
      tft.drawLine(centerX - rayLength/2, centerY + rayLength/2, centerX + rayLength/2, centerY - rayLength/2, flashColor);
      tft.fillCircle(centerX, centerY, 3, flashColor);
    } else if (anim->stage >= CAMERA_FLASH_TOTAL_STAGES) {
      anim->stage = 0;
      anim->active = false;
      *flashFlag = false;
      tft.fillRect(cameraX, cameraY, CAMERA_WIDTH, CAMERA_HEIGHT, TFT_WHITE);
      drawCamera(location);
    }
  }
}

void updateWasteRepoAnimation(Location location)
{
  AnimationState *anim = &_animStates[WASTE_REPO][location];
  bool *animateFlag = (location == Left) ? &_animateWasteRepoLeft : &_animateWasteRepoRight;
  bool *wasteRepoCompleted = (location == Left) ? &wasteRepoCompletedLeft : &wasteRepoCompletedRight;

  if (*animateFlag && !anim->active)
  {
    anim->active = true;
    anim->stage = 0;
    anim->lastTime = _currentTime;
    AnimationType *lastAnim = (location == Left) ? &lastAnimationLeft : &lastAnimationRight;
    *lastAnim = WASTE_REPO;
    
    // Activate pump relay
    if (location == Left) {
      activateRelay(RELAY_P1_PIN, PUMP_RELAY_ACTIVE_TIME_MS, &relayP1StartTime, &relayP1Active);
    } else {
      activateRelay(RELAY_P2_PIN, PUMP_RELAY_ACTIVE_TIME_MS, &relayP2StartTime, &relayP2Active);
    }
  }

  if (anim->active)
  {
    if (_currentTime - anim->lastTime >= WASTE_REPO_ANIM_STAGE_DURATION_MS)
    {
      anim->stage++;
      anim->lastTime = _currentTime;
      
      if (anim->stage >= WASTE_REPO_ANIM_TOTAL_STAGES)
      {
        anim->stage = 0;
        anim->active = false;
        *animateFlag = false;
        // Animation completed
        drawToilet(location); // Update toilet to show stage 5
      }
      drawWasteRepo(location);
    }
  }
}
void toggleTimers()
{
  if (!_drawTriangle) // Triangle clicked = start flush flow
  {
    initializeFlushFlow();
  }
  else // Square visible = stop flush flow
  {
    // Stop flush flow and timers
    _flushFlowActive = false;
    _leftFlushActive = false;
    _rightFlushActive = false;
    _timerLeftRunning = false;
    _timerRightRunning = false;
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