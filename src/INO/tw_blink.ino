
// ***
// * if for TWATCH_BARE, remember to change User_Setup_Select.h to use Setup45_TTGO_T_Watch.h
// ***


#if !defined(TWATCH_BARE)

  #include <LilyGoWatch.h>
  
#else

  #include "axp20x.h"
  #include <TFT_eSPI.h>

  #define I2C_BUS_SDA   21
  #define I2C_BUS_SCL   22
  
  #define TWATCH_TFT_BL (GPIO_NUM_15)
  #define BL_CHANNEL    0

  AXP20X_Class*   power = new AXP20X_Class();
  TFT_eSPI        tft   = TFT_eSPI();

  #include "focaltech.h"

  #define TOUCH_SDA     (GPIO_NUM_23)
  #define TOUCH_SCL     (GPIO_NUM_32)

  FocalTech_Class* touch = new FocalTech_Class();

#endif

void setup() {
  Serial.begin(115200);

#if !defined(TWATCH_BARE)

  // create the T-Watch API object 
  TTGOClass *ttgo = TTGOClass::getWatch();

  // init the T-Watch API object
  ttgo->begin();
  ttgo->openBL();

  // from the T-Watch API object, get the TFT_eSPI object
  TFT_eSPI& tft = *ttgo->tft;

#else

  // some power management initialization
  Wire.begin(I2C_BUS_SDA, I2C_BUS_SCL);
  power->begin();
  power->setPowerOutPut(AXP202_LDO2, AXP202_ON);

  // init TFT
  tft.init();

  // turn on backlight
  ledcSetup(BL_CHANNEL, 1000, 8);
  ledcAttachPin(TWATCH_TFT_BL, BL_CHANNEL);
  ledcWrite(0, 255);

  // init touch
  Wire1.begin(TOUCH_SDA, TOUCH_SCL);
  touch->begin(Wire1);

#endif

  tft.fillScreen(TFT_LIGHTGREY);
}

long lastMillis = 0;
bool on = true;

void loop() {

#if !defined(TWATCH_BARE)

  // get the T-Watch API object (only the first call will create) 
  TTGOClass *ttgo = TTGOClass::getWatch();

  // from the T-Watch API object, get the TFT_eSPI object
  TFT_eSPI& tft = *ttgo->tft;

#endif

  bool blink = false;
  if ((millis() - lastMillis) >= 1000) {
    blink = true;
    on = !on;
    lastMillis = millis();
  }

  if (blink) {
    tft.fillCircle(120, 120, 50, on ? TFT_RED : TFT_BLUE);
  }

#if !defined(TWATCH_BARE)

  int16_t x, y;
  if (ttgo->getTouch(x, y)) {
    Serial.println(String("- TOUCH: ") + x + " " + y);
  }

#else

  uint16_t x, y;
  if (touch->getPoint(x, y)) {
    Serial.println(String("- TOUCH: ") + x + " " + y);
  }

#endif

}