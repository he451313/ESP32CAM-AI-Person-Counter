#include <Arduino.h>
#include "esp_camera.h"
#include <base64.h>


#include <TensorFlowLite_ESP32.h>
#include "tensorflow/lite/micro/micro_error_reporter.h"
#include "tensorflow/lite/micro/all_ops_resolver.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "person_detect_model_data.h"


#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>


// ESP32-CAM 相機腳位
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// OLED I2C 腳位
#define I2C_SDA 13
#define I2C_SCL 15
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define SCREEN_ADDRESS 0x3C


// TFLite 變數
const tflite::Model* model = nullptr;
tflite::MicroInterpreter* interpreter = nullptr;
TfLiteTensor* input = nullptr;
TfLiteTensor* output = nullptr;
tflite::ErrorReporter* error_reporter = nullptr;
constexpr size_t kTensorArenaSize = 136 * 1024;
uint8_t* tensor_arena = nullptr;

// OLED 物件
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// 計數邏輯變數
int person_total_count = 0;
bool is_person_present = false; // 鎖定旗標，確保一人只計一次

void setup() {
  Serial.begin(460800);
  Serial.println("\n--- AI Person Counter with OLED ---");

  // 初始化 OLED 與 I2C 診斷
  Wire.begin(I2C_SDA, I2C_SCL);
  
  // I2C 掃描診斷邏輯
  Serial.println("Scanning I2C bus...");
  byte error, address;
  int nDevices = 0;
  for(address = 1; address < 127; address++ ) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();
    if (error == 0) {
      Serial.print("I2C device found at address 0x");
      Serial.println(address, HEX);
      nDevices++;
    }
  }
  if (nDevices == 0) {
    Serial.println("No I2C devices found! Check your wiring on GPIO 13 & 15.");
  }

  // 初始化 OLED
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("OLED SSD1306 allocation failed - Display not found."));
    // 這裡不 return，讓相機與 AI 繼續，看 Serial 就好
  } else {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0,10);
    display.println("AI System Loading...");
    display.display();
  }

  // 相機初始化
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_GRAYSCALE; 
  config.frame_size = FRAMESIZE_QQVGA;
  config.jpeg_quality = 12;
  config.fb_count = 1;

  if (esp_camera_init(&config) != ESP_OK) {
    Serial.println("Camera init failed!");
    return;
  }

  // TensorFlow Lite 初始化
  static tflite::MicroErrorReporter micro_error_reporter;
  error_reporter = &micro_error_reporter;
  model = tflite::GetModel(g_person_detect_model_data);
  tensor_arena = (uint8_t*)ps_malloc(kTensorArenaSize);
  static tflite::AllOpsResolver resolver;
  static tflite::MicroInterpreter static_interpreter(model, resolver, tensor_arena, kTensorArenaSize, error_reporter);
  interpreter = &static_interpreter;
  interpreter->AllocateTensors();
  input = interpreter->input(0);
  output = interpreter->output(0);

  if (nDevices > 0) {
    display.clearDisplay();
    display.setCursor(0,10);
    display.println("System Ready!");
    display.display();
  }
  Serial.println("System Ready.");
  delay(1000);
}

void loop() {
  static unsigned long last_loop_time = 0;
  unsigned long current_time = millis();
  float esp32_fps = (last_loop_time > 0) ? 1000.0 / (current_time - last_loop_time) : 0;
  last_loop_time = current_time;

  // 擷取影像
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) return;

  uint8_t* crop_buf = (uint8_t*)malloc(9216);
  if (!crop_buf) {
    esp_camera_fb_return(fb);
    return;
  }

  // 影像裁切與模型輸入準備 (96x96)
  int start_x = (160 - 96) / 2;
  int start_y = (120 - 96) / 2;
  for (int y = 0; y < 96; y++) {
    for (int x = 0; x < 96; x++) {
      uint8_t pixel = fb->buf[(start_y + y) * 160 + (start_x + x)];
      input->data.int8[y * 96 + x] = (int8_t)(pixel - 128);
      crop_buf[y * 96 + x] = pixel; 
    }
  }
  esp_camera_fb_return(fb);

  // 執行推論
  if (interpreter->Invoke() != kTfLiteOk) {
    free(crop_buf); 
    return;
  }

  // 辨識結果與計數邏輯
  int8_t person_score = output->data.int8[1];
  int person_prob = person_score + 128; 

  // 計數器觸發邏輯 
  // 機率 > 160 且處於非鎖定狀態
  if (person_prob > 160) {
    if (!is_person_present) {
      person_total_count++;
      is_person_present = true; // 鎖定
    }
  } else if (person_prob < 100) {
    is_person_present = false; // 解除鎖定
  }

  // OLED 畫面更新
  display.clearDisplay();
  
  // 第一行 狀態
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("AI: ");
  display.println(person_prob > 160 ? "DETECTED" : "Scanning");

  // 第二行 機率與 FPS
  display.setCursor(0, 15);
  display.print("Prob:");
  display.print(person_prob);
  display.print(" FPS:");
  display.println((int)esp32_fps);

  // 第三行 分割線
  display.drawLine(0, 26, 128, 26, SSD1306_WHITE);

  // 第四行 累積人數
  display.setTextSize(2);
  display.setCursor(0, 35);
  display.print("TOTAL: ");
  display.println(person_total_count);
  
  display.display();

  // 傳送封包給 Python
  String encoded_img = base64::encode(crop_buf, 9216);
  free(crop_buf); 

  Serial.println("[[FRAME_START]]");
  Serial.println(person_prob);
  Serial.println(esp32_fps);
  Serial.println(encoded_img);
  Serial.println("[[FRAME_END]]");

  delay(5); 
}
