#ifndef SANI_LOGO_H
#define SANI_LOGO_H

#include <TFT_eSPI.h>
#include "Shapes.h"

// External references to global variables and objects
extern TFT_eSPI tft;
extern const int SCREEN_WIDTH;
extern const int SCREEN_HEIGHT;
extern const int LOGO_WIDTH;
extern const int LOGO_HEIGHT;
extern Shape* _saniLogoShape;

// Function prototype
void drawSaniLogo();

#endif // SANI_LOGO_H