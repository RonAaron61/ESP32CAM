#include <WiFi.h>
#include "esp_camera.h"
#include "Arduino.h"
#include "FS.h"                // SD Card ESP32
#include "SD_MMC.h"            // SD Card ESP32
#include "soc/soc.h"           // Disable brownour problems
#include "soc/rtc_cntl_reg.h"  // Disable brownour problems
#include "driver/rtc_io.h"

// Pin definition for CAMERA_MODEL_AI_THINKER
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

int pictureNumber = 0;
#define BUTTON_PIN 1
#define FLASH_PIN 3

bool cameraInitialized = false;
bool sdInitialized = false;

bool initializeCamera() {
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
  config.pixel_format = PIXFORMAT_JPEG; 
  config.grab_mode = CAMERA_GRAB_LATEST;
  
  if(psramFound()){
    config.frame_size = FRAMESIZE_UXGA; // FRAMESIZE_ + QVGA|CIF|VGA|SVGA|XGA|SXGA|UXGA
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }
  
  // Init Camera
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return false;
  }

  // Optimasi untuk warna yang lebih natural
  sensor_t * s = esp_camera_sensor_get();
  s->set_brightness(s, 1);     // -2 to 2
  s->set_contrast(s, 0);       // -2 to 2
  s->set_saturation(s, 0);     // -2 to 2 (0 = normal saturation)
  s->set_special_effect(s, 0); // 0: No Effect (color)
  s->set_whitebal(s, 1);       // 1 = enable white balance
  s->set_awb_gain(s, 1);       // 1 = enable AWB gain
  s->set_wb_mode(s, 0);        // 0 – Auto, 1 – Sunny, 2 – Cloudy, 3 – Office, 4 – Home
  s->set_exposure_ctrl(s, 1);  // 1 = enable exposure control
  s->set_aec2(s, 1);           // 0 = disable AEC2
  s->set_ae_level(s, 0);       // -2 to 2
  s->set_aec_value(s, 300);    // 0 to 1200
  s->set_gain_ctrl(s, 1);      // 1 = enable gain control
  s->set_agc_gain(s, 0);       // 0 to 30
  s->set_gainceiling(s, (gainceiling_t)5);  // 0 to 6
  s->set_bpc(s, 1);            // 1 = enable black pixel correction 
  s->set_wpc(s, 1);            // 1 = enable white pixel correction
  s->set_raw_gma(s, 1);        // 1 = enable gamma correction
  s->set_lenc(s, 1);           // 1 = enable lens correction
  s->set_hmirror(s, 0);        // 0 = disable mirror
  s->set_vflip(s, 0);          // 0 = disable flip
  s->set_dcw(s, 1);            // 1 = enable DCW
  s->set_colorbar(s, 0);       // 0 = disable color bar test pattern
  
  cameraInitialized = true;
  Serial.println("Camera initialized successfully");
  return true;
}

bool initializeSDCard() {
  Serial.println("Starting SD Card");
  if(!SD_MMC.begin()){
    Serial.println("SD Card Mount Failed");
    return false;
  }
  
  uint8_t cardType = SD_MMC.cardType();
  if(cardType == CARD_NONE){
    Serial.println("No SD Card attached");
    return false;
  }
  
  Serial.println("SD Card initialized successfully");
  sdInitialized = true;
  return true;
}

void calculatePict(){
  int photoCount = 0;
  File root = SD_MMC.open("/");
  File file = root.openNextFile();
  while(file){
    if(!file.isDirectory() && String(file.name()).endsWith(".jpg")) {
      photoCount++;
    }
    file = root.openNextFile();
  }

  pictureNumber = photoCount + 1;
  Serial.println (photoCount);
}

void takePicture() {
  if (!cameraInitialized || !sdInitialized) {
    Serial.println("Camera or SD Card not initialized!");
    return;
  }
  
  digitalWrite(FLASH_PIN, HIGH);
  delay(50);

  camera_fb_t * fb = NULL;
  //camera_fb_t * fb = esp_camera_fb_get();

  //esp_camera_fb_return(fb); // dispose the buffered image
  //fb = NULL; // reset to capture errors
  //fb = esp_camera_fb_get();
  
  Serial.println("Taking picture...");
  fb = esp_camera_fb_get();  
  if(!fb) {
    Serial.println("Camera capture failed");
    return;
  }

  delay(10);

  String path = "/picture" + String(pictureNumber) +".jpg";
  pictureNumber++;

  fs::FS &fs = SD_MMC; 
  Serial.printf("Picture file name: %s\n", path.c_str());
  
  File file = fs.open(path.c_str(), FILE_WRITE);
  if(!file){
    Serial.println("Failed to open file in writing mode");
  } 
  else {
    file.write(fb->buf, fb->len);
    Serial.printf("Saved file to path: %s\n", path.c_str());

  }
  file.close();
  delay(10);
  esp_camera_fb_return(fb); 
  
  digitalWrite(FLASH_PIN, LOW);
  digitalWrite(4, LOW);
  Serial.println("Picture saved successfully!");
}


void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector
 
  Serial.begin(115200);
  Serial.println("ESP32-CAM Started - Press button to take picture");
  
  // Setup tombol
  pinMode(BUTTON_PIN, INPUT_PULLDOWN);
  pinMode(FLASH_PIN, OUTPUT);
  pinMode(4, OUTPUT);
  
  // Initialize camera dan SD card sekali di setup
  if (!initializeCamera()) {
    Serial.println("Failed to initialize camera!");
    return;
  }

  delay(10);

  if (!initializeSDCard()) {
    Serial.println("Failed to initialize SD Card!");
    return;
  }
  
  delay(10);
  
  calculatePict();
  
  Serial.println("System ready - Press button to take picture");
}

void loop() {
  // Cek jika tombol ditekan5
  
 if (digitalRead(BUTTON_PIN) == HIGH) {
   Serial.println("Button pressed - taking picture...");
   takePicture();
   
   // Delay untuk debounce
   delay(200);
 }
  
  delay(100);
}
