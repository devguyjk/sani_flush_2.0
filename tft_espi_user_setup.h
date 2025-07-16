// User_Setup.h configuration for ESP32-S3 and ILI9341
// Place this in: Arduino/libraries/TFT_eSPI/User_Setup.h

//#define ST7735_GREENTAB128    // For 128 x 128 display
#define TOUCH_DRIVER XPT2046_DRIVER
#define SUPPORT_TRANSACTIONS

// See SetupX_Template.h for all options available
#define ILI9341_DRIVER

#define TFT_WIDTH  240
#define TFT_HEIGHT 320
                    
#define TFT_CS   10
#define TFT_MOSI 11
#define TFT_SCLK 12 
#define TFT_MISO 13 
#define TFT_DC    7
#define TFT_RST   6

// XPT2046 TouchPins
#define TOUCH_CS 9

// Enable touch support
#define TOUCH_ENABLE

// Backlight control (optional)
#define TFT_BL   4 // Example: Connect LED/BLK to GPIO4 for brightness control
#define TFT_BACKLIGHT_ON HIGH // or LOW, depending on your module's logic

#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF

#define SMOOTH_FONT

// FSPI (or VSPI) port (SPI2) used unless following defined. HSPI port is (SPI3) on S3.
#define USE_HSPI_PORT

//#define SPI_FREQUENCY  27000000
#define SPI_FREQUENCY  40000000   // Maximum for ILI9341
#define SPI_READ_FREQUENCY  6000000 // 6 MHz is the maximum SPI read speed for the ST7789V
#define SPI_TOUCH_FREQUENCY 2500000

// Color depth
#define TFT_RGB_ORDER TFT_BGR  // Colour order Blue-Green-Red

