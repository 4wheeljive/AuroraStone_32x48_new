// Basic framework ******************************************************************************
#include <Arduino.h>
#include <FastLED.h>
#include "fl/xymap.h"
#include "matrixMap_32x48_3pin.h"
#include "palettes.h"

#include <Preferences.h>
Preferences preferences;

#define HEIGHT 32 
#define WIDTH 48
#define NUM_LEDS ( WIDTH * HEIGHT )

#define NUM_SEGMENTS 3
#define NUM_LEDS_PER_SEGMENT 512

CRGB leds[NUM_LEDS];
uint16_t ledNum;

#define DATA_PIN_1 2
#define DATA_PIN_2 3
#define DATA_PIN_3 4

using namespace fl;

//bleControl variables ************************************************************************
//elements that must be set before #include "bleControl.h" 

uint8_t program = 1;
uint8_t pattern;
bool displayOn = true;
bool runPride = true;
bool runWaves = false;
bool rotateWaves = true; 

extern const TProgmemRGBGradientPaletteRef gGradientPalettes[]; 
extern const uint8_t gGradientPaletteCount;

uint8_t gCurrentPaletteNumber;
CRGBPalette16 gCurrentPalette;
CRGBPalette16 gTargetPalette;

uint8_t SPEED;
float speedfactor;
uint8_t BRIGHTNESS;
const uint8_t brightnessInc = 15;
bool brightnessChanged = false;

#include "bleControl.h"

// Miscellaneous *****************************************************************************

uint8_t blendFract = 64;
uint16_t hueIncMax = 1500;
CRGB newcolor = CRGB::Black;

uint8_t savedSpeed;
uint8_t savedBrightness;

#define SECONDS_PER_PALETTE 20

#define SERPENTINE true 

bool screenTest = false;

//*******************************************************************************************

void setup() {

  preferences.begin("settings", true); // true == read only mode
    savedBrightness  = preferences.getUChar("brightness");
    savedSpeed  = preferences.getUChar("speed");
  preferences.end();

  //BRIGHTNESS = 155;
  //SPEED = 5;
  BRIGHTNESS = savedBrightness;
  SPEED = savedSpeed;

  if (screenTest) {
    XYMap screenMap(WIDTH, HEIGHT, SERPENTINE);
    FastLED.addLeds<WS2812B, DATA_PIN_1, GRB>(leds, NUM_LEDS)
      .setScreenMap(screenMap); 
  }

  else {
    FastLED.addLeds<WS2812B, DATA_PIN_1, GRB>(leds, 0, NUM_LEDS_PER_SEGMENT)
      .setCorrection(TypicalLEDStrip);

    FastLED.addLeds<WS2812B, DATA_PIN_2, GRB>(leds, NUM_LEDS_PER_SEGMENT, NUM_LEDS_PER_SEGMENT)
      .setCorrection(TypicalLEDStrip);
   
    FastLED.addLeds<WS2812B, DATA_PIN_3, GRB>(leds, NUM_LEDS_PER_SEGMENT * 2, NUM_LEDS_PER_SEGMENT)
      .setCorrection(TypicalLEDStrip);
  }
 FastLED.setBrightness(BRIGHTNESS);

 FastLED.clear();
 FastLED.show();

/* Serial.begin(115200);
 delay(500);
 Serial.print("Initial brightness: ");
 Serial.println(BRIGHTNESS);
 Serial.print("Initial speed: ");
 Serial.println(SPEED);
*/

 bleSetup();

}

//*******************************************************************************************

uint16_t ledMap (uint8_t x, uint8_t y){
  uint16_t j = (y * WIDTH) + x;
  return j;
}

//*******************************************************************************************

void updateSettings_brightness(uint8_t newBrightness){
 preferences.begin("settings",false);  // false == read write mode
   preferences.putUChar("brightness", newBrightness);
 preferences.end();
 savedBrightness = newBrightness;
 Serial.println("Brightness setting updated");
}

//*******************************************************************************************

void updateSettings_speed(uint8_t newSpeed){
 preferences.begin("settings",false);  // false == read write mode
   preferences.putUChar("speed", newSpeed);
 preferences.end();
 savedSpeed = newSpeed;
 Serial.println("Speed setting updated");
}

// PRIDE/WAVES******************************************************************************
// Code matrix format: 1D, Serpentine

void prideWaves(uint8_t pattern) {

  static uint16_t sPseudotime = 0;
  static uint16_t sLastMillis = 0;
  static uint16_t sHue16 = 0;
 
  uint8_t sat8 = beatsin88( 87, 240, 255); 
  uint8_t brightdepth = beatsin88( 341, 96, 224);
  uint16_t brightnessthetainc16 = beatsin88( 203, (25 * 256), (40 * 256));
  uint8_t msmultiplier = beatsin88(147, 23, 60); 
 
  uint16_t hue16 = sHue16; 
  uint16_t hueinc16 = beatsin88(113, 1, 3000 );    
  uint16_t ms = millis();  
  uint16_t deltams = ms - sLastMillis ;
  sLastMillis  = ms;     
  sPseudotime += deltams * msmultiplier*speedfactor;
  sHue16 += deltams * beatsin88( 400, 5,9);
  uint16_t brightnesstheta16 = sPseudotime;

  for( uint16_t i = 0 ; i < NUM_LEDS; i++ ) {
   
    hue16 += hueinc16;
    uint8_t hue8 = hue16 / 256;

    if (runWaves) {
      uint16_t h16_128 = hue16 >> 7;
      if( h16_128 & 0x100) {
        hue8 = 255 - (h16_128 >> 1);
      } else {
        hue8 = h16_128 >> 1;
      }
    }

    brightnesstheta16  += brightnessthetainc16;
    uint16_t b16 = sin16( brightnesstheta16  ) + 32768;

    uint16_t bri16 = (uint32_t)((uint32_t)b16 * (uint32_t)b16) / 65536;
    uint8_t bri8 = (uint32_t)(((uint32_t)bri16) * brightdepth) / 65536;
    bri8 += (255 - brightdepth);
  
    switch (pattern) {
  
      case 1:
        newcolor = CHSV( hue8, sat8, bri8);
        blendFract = 64;
        break;
  
      case 2:
        uint8_t index = hue8;
        index = scale8( index, 240);
        newcolor = ColorFromPalette( gCurrentPalette, index, bri8);
        blendFract = 128;
        break;
   }
    
    if (screenTest) {
      ledNum = i;
      //ledNum = (NUM_LEDS-1) - ledNum;
    }
    else {
      ledNum = loc2indSerp[i];;
    }

    nblend( leds[ledNum], newcolor, blendFract);

  }

}

// RAINBOW MATRIX ******************************************************************************
// Code matrix format: 2D, Progressive by rows

void DrawOneFrame( uint8_t startHue8, int8_t yHueDelta8, int8_t xHueDelta8)
{
  uint8_t lineStartHue = startHue8;
  for( uint8_t y = 0; y < HEIGHT; y++) {
    lineStartHue += yHueDelta8;
    uint8_t pixelHue = lineStartHue;      
    for( uint8_t x = 0; x < WIDTH; x++) {
      pixelHue += xHueDelta8;
      if (screenTest) {
        leds[ledMap(x,y)] = CHSV(pixelHue, 255, 255);
      }
      else {
        leds[loc2indProgbyRow[y][x]] = CHSV(pixelHue, 255, 255);
      }  
    }
  }
}

void rainbowMatrix () 
{
    uint32_t ms = millis();
    int32_t yHueDelta32 = ((int32_t)cos16( ms * (27/1) ) * (350 / WIDTH));
    int32_t xHueDelta32 = ((int32_t)cos16( ms * (39/1) ) * (310 / HEIGHT));
    DrawOneFrame( ms / 65536, yHueDelta32 / 32768, xHueDelta32 / 32768);
 }

//*******************************************************************************************

void loop() {
   
   switch(program){

     case 0:  
       rainbowMatrix();
       nscale8(leds,NUM_LEDS,BRIGHTNESS);
       break; 

      case 1:
 
        if (runPride) { 
           hueIncMax = 3000;
           prideWaves(1); 
        }
   
        if (runWaves) { 
          hueIncMax = 1500;
          if (rotateWaves) {
            EVERY_N_SECONDS( SECONDS_PER_PALETTE ) {
              gCurrentPaletteNumber = addmod8( gCurrentPaletteNumber, 1, gGradientPaletteCount);
              gTargetPalette = gGradientPalettes[ gCurrentPaletteNumber ];
              pPaletteCharacteristic->setValue(String(gCurrentPaletteNumber).c_str());
              pPaletteCharacteristic->notify();
              Serial.print("Color palette: ");
              Serial.println(gCurrentPaletteNumber);
            }
          }
          EVERY_N_MILLISECONDS(40) {
             nblendPaletteTowardPalette( gCurrentPalette, gTargetPalette, 16); 
          }
          prideWaves(2); 
        }
        break; 
    }

   FastLED.show();
 }

// **************************************************************************************************