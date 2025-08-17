#ifndef DRAW_FUNCTIONS_H
#define DRAW_FUNCTIONS_H

#include <TFT_eSPI.h>
#include "Shapes.h"
#include "global_vars.h"

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
void drawLeftFlushBar();
void updateLeftFlushBar();
void drawRightFlushBar();
void updateRightFlushBar();
String callUploadSaniPhoto(const char *cameraID, const char *imagePrefix);
void captureDualCameras(Location location, bool isAuto);
void updatePendingCaptures();

#endif // DRAW_FUNCTIONS_H