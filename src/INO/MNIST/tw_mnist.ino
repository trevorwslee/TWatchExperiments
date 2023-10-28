// ***
// * if for TWATCH_BARE, remember to change User_Setup_Select.h to use Setup45_TTGO_T_Watch.h
// ***


#if !defined(TWATCH_BARE)

 #include <LilyGoWatch.h>

  #define BG_COLOR    TFT_SILVER
  #define DOT_COLOR   TFT_NAVY

#else

  #include "axp20x.h"
  #define I2C_BUS_SDA   21
  #define I2C_BUS_SCL   22
  
  #define TWATCH_TFT_BL (GPIO_NUM_15)
  #define BL_CHANNEL    0

  #define MOTOR_PIN     4
  #define MOTOR_CHANNEL 1

  AXP20X_Class*   power = new AXP20X_Class();

  #include <TFT_eSPI.h> // Graphics and font library for ST7735 driver chip
  TFT_eSPI        tft   = TFT_eSPI();

  #include "focaltech.h"
  #define TOUCH_SDA       (GPIO_NUM_23)
  #define TOUCH_SCL       (GPIO_NUM_32)

  FocalTech_Class* touch = new FocalTech_Class();


  #define BG_COLOR    TFT_SILVER
  #define DOT_COLOR   TFT_DARKGREEN

#endif


#define TFT_WIDTH   240
#define TFT_HEIGHT  240


#include <TensorFlowLite_ESP32.h>
#include "tensorflow/lite/micro/all_ops_resolver.h"
#include "tensorflow/lite/micro/micro_error_reporter.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/schema/schema_generated.h"
// *****
// * make sure signature of the model is
// *   const unsigne char mnist_model_tflite[] = ...
// *****
#include "mnist_model.h"
// error reporter for TensorFlow Lite
class DDTFLErrorReporter : public tflite::ErrorReporter {
public:
  virtual int Report(const char* format, va_list args) {
    int len = strlen(format);
    char buffer[max(32, 2 * len)];  // assume 2 times format len is big enough
    sprintf(buffer, format, args);
    Serial.println(buffer);
    return 0;
  }
};
tflite::ErrorReporter* error_reporter = new DDTFLErrorReporter();
const tflite::Model* model = ::tflite::GetModel(mnist_model_tflite);
const int tensor_arena_size = 81 * 1024;  // guess
uint8_t* tensor_arena;
tflite::MicroInterpreter* interpreter = NULL;
TfLiteTensor* input;



#define PREDICT_WAIT_MS               1000
#define SCREEN_OFF_WAIT_MS            30000
#define TWATCH_OFF_WAIT_MS            60000


void setup() {
  Serial.begin(115200);

#if !defined(TWATCH_BARE)

  TTGOClass *ttgo = TTGOClass::getWatch();
  ttgo->begin();
  ttgo->openBL();
  ttgo->motor_begin();

  TFT_eSPI& tft = *ttgo->tft;

#else

  // without messing with power, the screen will not light up after reboot
  Wire.begin(I2C_BUS_SDA, I2C_BUS_SCL);
  Serial.println("AXP Power ...");
  int ret = power->begin();
  if (ret == AXP_FAIL) {
      Serial.println("... AXP Power begin failed");
  }
  ret = power->setPowerOutPut(AXP202_LDO2, AXP202_ON);
  if (ret == AXP_FAIL) {
      Serial.println("... setPowerOutPut failed");
  }
  if (ret == AXP_PASS) {
    Serial.println("... done AXP Power");
  }

  ledcSetup(BL_CHANNEL, 1000, 8);
  ledcAttachPin(TWATCH_TFT_BL, BL_CHANNEL);
  ledcWrite(0, 255);

#endif  


  tft.init();
  tft.setRotation(2);
  tft.fillScreen(BG_COLOR);
  tft.setTextSize(4);
  tft.setTextColor(TFT_RED, TFT_CYAN);

#if defined(TWATCH_BARE)

  // touch screen
  Wire1.begin(TOUCH_SDA, TOUCH_SCL);
  touch->begin(Wire1);

  // motor
  ledcSetup(MOTOR_CHANNEL, 1000, 8);
  ledcAttachPin(MOTOR_PIN, MOTOR_CHANNEL);

#endif


  // check version to make sure supported
  if (model->version() != TFLITE_SCHEMA_VERSION) {
    error_reporter->Report("Model provided is schema version %d not equal to supported version %d.",
    model->version(), TFLITE_SCHEMA_VERSION);
  }

  // allocation memory for tensor_arena ... in similar fashion as espcam_person.ino
  tensor_arena = (uint8_t *) heap_caps_malloc(tensor_arena_size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (tensor_arena == NULL) {
    error_reporter->Report("heap_caps_malloc() failed");
    return;
  }

  // pull in all the operation implementations
  tflite::AllOpsResolver* resolver = new tflite::AllOpsResolver();

  // build an interpreter to run the model with
  interpreter = new tflite::MicroInterpreter(model, *resolver, tensor_arena, tensor_arena_size, error_reporter);

  // allocate memory from the tensor_arena for the model's tensors
  TfLiteStatus allocate_status = interpreter->AllocateTensors();
  if (allocate_status != kTfLiteOk) {
    error_reporter->Report("AllocateTensors() failed");
    return;
  }

  // obtain a pointer to the model's input tensor
  input = interpreter->input(0);

  Serial.println("READY");
}


int lastX = -1;
int lastY = -1;
long lastMs = -1;
bool Pixels[28][28];  // 28x28 pixels data; x then y


long lastTouchMs = millis();
bool displaySleeping = false;

void ResetPixels() {
  lastX = -1;
  lastY = -1;
  lastMs = -1;
  for (int x = 0; x < 28; x++) {
    for (int y = 0; y < 28; y++) {
      Pixels[x][y] = false;
    }
  }
}



void loop() {
#if !defined(TWATCH_BARE)

  TTGOClass *ttgo = TTGOClass::getWatch();
  TFT_eSPI& tft = *ttgo->tft;

#endif

#if !defined(TWATCH_BARE)
  int16_t x, y;
  bool gotPoint = ttgo->getTouch(x, y);
#else
  uint16_t x, y;
  bool gotPoint = touch->getPoint(x, y);
#endif
  if (gotPoint) {
    lastTouchMs = millis();
    if (displaySleeping) {
      displaySleeping = false;
      Serial.println("DISPLAY ON");
#if !defined(TWATCH_BARE)
      ttgo->setBrightness(255);
#else
      ledcWrite(BL_CHANNEL, 255);
#endif
    }

    if (lastX != x || lastY != y) {
      int pixelX = min(27, (int) (0.5 + (28.0 * x / TFT_WIDTH)));
      int pixelY = min(27, (int) (0.5 + (28.0 * y / TFT_HEIGHT)));
      Pixels[pixelX][pixelY] = true;
      //Serial.println(String(". pixel_x: ") + String(pixelX) + String(" pixel_y: ") + String(pixelY));

      if (lastX == -1) {
        tft.fillScreen(BG_COLOR);
      }
      tft.fillCircle(x, y, 15, DOT_COLOR);
      lastX = x;
      lastY = y;
      lastMs = millis();
    }
  }

  if (lastMs != -1) {
      long diffMs = millis() - lastMs;
      if (diffMs >= PREDICT_WAIT_MS) {
        tft.fillScreen(TFT_WHITE);

      // set the pixels as the model's input
      int idx = 0;
      for (int y = 0; y < 28; y++) {
        for (int x = 0; x < 28; x++) {
          input->data.f[idx++] = Pixels[x][y] ? 255.0 : 0.0; 
        }
      }

      // run the model on this input and make sure it succeeds
      long detect_start_millis = millis();
      TfLiteStatus invoke_status = interpreter->Invoke();
      if (invoke_status != kTfLiteOk) {
        error_reporter->Report("Invoke failed");
      }
      long detect_taken_millis = millis() - detect_start_millis;

      // process the inference (person detection) results
      TfLiteTensor* output = interpreter->output(0);
      if (true) {
        for (int i = 0; i < 10; i++) {
          float p = output->data.f[i];
          Serial.println(String(". ") + String(i) + ": " + String(p, 3));
        }
        Serial.println(String(">>> in ") + detect_taken_millis + " ms");
      }

      int best = -1;
      float bestProp;
      for (int i = 0; i < 10; i++) {
        float prop = output->data.f[i];
        if (i == 0) {
          best = 0;
          bestProp = prop; 
        } else if (prop > bestProp) {
          best = i;
          bestProp = prop;
        }
      }

      ResetPixels();

      if (best != -1) {
        Serial.println(String("*** BEST: ") + String(best));
        int x = 10;
        int y = (TFT_HEIGHT / 2) - (2 * x);
        tft.setCursor(x, y);
        tft.println(String("LIKELY: ") + String(best));
#if !defined(TWATCH_BARE)
        ttgo->motor->onec(50);
#else
      ledcWriteTone(MOTOR_CHANNEL, 1000);
      delay(50);
      ledcWriteTone(MOTOR_CHANNEL, 0);
#endif
      } else {
        Serial.println("not predicted");
      }
    }
  }

  long diffMs = millis() - lastTouchMs;
  if (diffMs >= TWATCH_OFF_WAIT_MS) {
    Serial.println("TWATCH OFF");
#if !defined(TWATCH_BARE)
    ttgo->powerOff();
#else
    power->setPowerOutPut(AXP202_EXTEN, false);
    power->setPowerOutPut(AXP202_LDO4, false);
    power->setPowerOutPut(AXP202_DCDC2, false);
    power->setPowerOutPut(AXP202_LDO3, false);
    power->setPowerOutPut(AXP202_LDO2, false);
#endif
    Serial.println("GOING TO SLEEP ...");
    esp_sleep_enable_ext1_wakeup(GPIO_SEL_38, ESP_EXT1_WAKEUP_ALL_LOW);  // TOUCH SCREEN
    esp_deep_sleep_start();
    // will not reach here
  }
  if (!displaySleeping) {
    if (diffMs >= SCREEN_OFF_WAIT_MS) {
#if !defined(TWATCH_BARE)
      ttgo->setBrightness(8);
#else
      ledcWrite(BL_CHANNEL, 8);
#endif
      Serial.println("DISPLAY OFF");
      displaySleeping = true;
    }
  }
}