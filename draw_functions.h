#ifndef DRAW_FUNCTIONS_H
#define DRAW_FUNCTIONS_H

#include <TFT_eSPI.h>
#include "Shapes.h"
#include "global_vars.h"

// Global logging function
void writeLog(const char *format, ...);

// Function prototypes
void drawSaniLogo();
void drawStartStopButton();
void drawHamburger();
void drawCamera(Location location);
void drawToilet(Location location);
void drawFlushTimer(Location location);
void drawWasteRepo(Location location);
void drawMainDisplay();
void updateAnimations();
void updateToiletAnimation(Location location);
void updateCameraFlashAnimation(Location location);
void updateWasteRepoAnimation(Location location);
void toggleTimers();
void updateTimers();
void initializeFlushFlow();
void flushToilet(Location location);
void updateFlushFlow();
void drawFlowDetails();
void updateDuration(); // Update only duration line in flow details
void drawLeftFlushBar();
void updateLeftFlushBar();
void drawRightFlushBar();
void updateRightFlushBar();
void captureDualCameras(Location location, bool isAuto);
void updatePendingCaptures();
void updateCameraDelays();
void incrementLeftFlushCounter();
void incrementRightFlushCounter();
void incrementImageCounter();
void generateFlushCountString(char* buffer, size_t bufferSize);
void generateDurationString(char* buffer, size_t bufferSize);
float calculateTotalGallons(); // Shared gallon calculation function

// Timer recalibration functions
void recalibrateWorkflowTiming();
void validateFlushCounts();
unsigned long getRealTimeMillis();

#endif // DRAW_FUNCTIONS_H