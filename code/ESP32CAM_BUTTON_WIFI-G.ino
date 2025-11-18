#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "esp_camera.h"
#include "Arduino.h"
#include "FS.h"                // SD Card ESP32
#include "SD_MMC.h"            // SD Card ESP32
#include "soc/soc.h"           // Disable brownour problems
#include "soc/rtc_cntl_reg.h"  // Disable brownour problems
#include "driver/rtc_io.h"

// WiFi Credentials
const char* ssid = "ESP32_CAM_2";
const char* password = "123456789";

// Initialize AsyncWebServer on port 80
AsyncWebServer server(80);

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

// Fungsi untuk mendapatkan daftar file
String getFileList() {
  String fileList = "";
  int fileCount = 0;
  
  File root = SD_MMC.open("/");
  if(!root){
    return "<p>Failed to open directory</p>";
  }
  
  if(!root.isDirectory()){
    return "<p>Not a directory</p>";
  }
  
  File file = root.openNextFile();
  while(file){
    if(!file.isDirectory()){
      String fileName = String(file.name());
      if(fileName.endsWith(".jpg")) {
        fileCount++;
        fileList += "<div class='file-item'>";
        fileList += "<span class='file-name'>" + fileName + "</span>";
        fileList += "<span class='file-size'>(" + String(file.size()) + " bytes)</span>";
        fileList += "<div class='file-actions'>";
        fileList += "<a class='btn view-btn' href='/view/" + fileName + "' target='_blank'>View</a>";
        fileList += "<a class='btn download-btn' href='/download/" + fileName + "' download>Download</a>";
        fileList += "<a class='btn delete-btn' href='/delete/" + fileName + "' onclick='return confirm(\"Delete " + fileName + "?\")'>Delete</a>";
        fileList += "</div>";
        fileList += "</div>";
      }
    }
    file = root.openNextFile();
  }
  
  if(fileCount == 0) {
    fileList = "<p>No photos found</p>";
  }
  
  return fileList;
}

void deleteAllPhotos() {
  File root = SD_MMC.open("/");
  File file = root.openNextFile();
  int deletedCount = 0;
  
  while(file){
    if(!file.isDirectory()){
      String fileName = String(file.name());
      if(fileName.endsWith(".jpg")) {
        if (SD_MMC.remove(fileName.c_str())){
          Serial.println("Deleted: " + fileName);
        }
        else {
          Serial.println("Fail Delete" + fileName);
        }
        deletedCount++;
      }
    }
    file = root.openNextFile();
  }
  
  calculatePict();

  Serial.printf("Deleted %d photos\n", deletedCount);
}

// HTML Page
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
  <title>ESP32-CAM Photo Gallery</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body {
      font-family: Arial, sans-serif;
      text-align: center;
      margin: 20px;
      background-color: #f0f0f0;
    }
    .container {
      max-width: 800px;
      margin: 0 auto;
      background: white;
      padding: 20px;
      border-radius: 10px;
      box-shadow: 0 0 10px rgba(0,0,0,0.1);
    }
    .file-item {
      margin: 10px 0;
      padding: 15px;
      border: 1px solid #ddd;
      border-radius: 5px;
      background: #f9f9f9;
      text-align: left;
    }
    .file-name {
      font-weight: bold;
      display: block;
    }
    .file-size {
      color: #666;
      font-size: 0.9em;
    }
    .file-actions {
      margin-top: 10px;
    }
    .btn {
      padding: 8px 15px;
      text-decoration: none;
      border-radius: 4px;
      margin: 2px;
      display: inline-block;
      font-size: 0.9em;
    }
    .view-btn {
      background: #2196F3;
      color: white;
    }
    .download-btn {
      background: #4CAF50;
      color: white;
    }
    .delete-btn {
      background: #f44336;
      color: white;
    }
    .header {
      margin-bottom: 20px;
    }
    .action-btn {
      background: #FF9800;
      color: white;
      padding: 10px 20px;
      text-decoration: none;
      border-radius: 5px;
      margin: 5px;
      display: inline-block;
    }
    .info {
      background: #e7f3ff;
      padding: 10px;
      border-radius: 5px;
      margin: 10px 0;
    }
  </style>
</head>
<body>
  <div class="container">
    <div class="header">
      <h1>ESP32-CAM Photo Gallery</h1>
      <div class="info">      
        <p>Total photos: <strong>%PHOTO_COUNT%</strong></p>
      </div>
      <a class="action-btn" href="/">Refresh</a>
      <a class="action-btn" href="/take">Take New Photo</a>
      <a class="action-btn" href="/delete-all" onclick="return confirm('Are you sure you want to delete ALL photos?')">Delete All Photos</a>
    </div>
    <div id="file-list">
      %FILE_LIST%
    </div>
  </div>
</body>
</html>
)rawliteral";


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
  
  // Uncomment this if you want to use higher spec
  // if(psramFound()){
  //   config.frame_size = FRAMESIZE_UXGA; // FRAMESIZE_ + QVGA|CIF|VGA|SVGA|XGA|SXGA|UXGA
  //   config.jpeg_quality = 10;
  //   config.fb_count = 2; 
  // } else {
  //   config.frame_size = FRAMESIZE_SVGA;
  //   config.jpeg_quality = 12;
  //   config.fb_count = 1;
  // }

  // Set the image size manually here
  config.frame_size = FRAMESIZE_SVGA;
  config.jpeg_quality = 12;
  config.fb_count = 1;
  
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
  s->set_special_effect(s, 0); // 0:No Effect (color)
  s->set_whitebal(s, 1);       // 1 = enable white balance
  s->set_awb_gain(s, 1);       // 1 = enable AWB gain
  s->set_wb_mode(s, 1);        // 0: Auto white balance
  s->set_exposure_ctrl(s, 1);  // 1 = enable exposure control
  s->set_aec2(s, 0);           // 0 = disable AEC2
  s->set_ae_level(s, 0);       // -2 to 2
  s->set_aec_value(s, 300);    // 0 to 1200
  s->set_gain_ctrl(s, 1);      // 1 = enable gain control
  s->set_agc_gain(s, 0);       // 0 to 30
  s->set_gainceiling(s, (gainceiling_t)6);  // 0 to 6
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
  
  Serial.println("Taking picture...");
  fb = esp_camera_fb_get();  
  delay(50);

  if(!fb) {
    Serial.println("Camera capture failed");
    return;
  }
  
  pictureNumber++;
  String path = "/picture" + String(pictureNumber) +".jpg";

  fs::FS &fs = SD_MMC; 
  Serial.printf("Picture file name: %s\n", path.c_str());
  
  File file = fs.open(path.c_str(), FILE_WRITE);

  if(!file){
    delay(200);
    Serial.println("Failed to open file in writing mode");
    file = fs.open(path.c_str(), FILE_WRITE);
  }
 
  else {
    file.write(fb->buf, fb->len);
    Serial.printf("Saved file to path: %s\n", path.c_str());

  }
  digitalWrite(FLASH_PIN, LOW);
  file.close();
  esp_camera_fb_return(fb); 
  
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
  
  // Initialize camera and SD card
  if (!initializeCamera()) {
    Serial.println("Failed to initialize camera!");
    return;
  }
  
  if (!initializeSDCard()) {
    Serial.println("Failed to initialize SD Card!");
    return;
  }
  
  calculatePict();

  // Start WiFi SoftAP
  WiFi.softAP(ssid, password, 11);  //Channel 11

  // Setup Web Server Routes
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    String html = String(index_html);
    html.replace("%FILE_LIST%", getFileList());

    calculatePict();
    html.replace("%PHOTO_COUNT%", String(pictureNumber));
    
    request->send(200, "text/html", html);
  });

  // Route to show image in full size
  server.on("/view/*", HTTP_GET, [](AsyncWebServerRequest *request){
    String path = request->url();
    path.replace("/view/", "/");
    
    if(SD_MMC.exists(path)) {
      request->send(SD_MMC, path, "image/jpeg");
    } else {
      request->send(404, "text/plain", "File not found");
    }
  });

  // Route to download file
  server.on("/download/*", HTTP_GET, [](AsyncWebServerRequest *request){
    String path = request->url();
    path.replace("/download/", "/");
    
    if(SD_MMC.exists(path)) {
      request->send(SD_MMC, path, "image/jpeg", true);
    } else {
      request->send(404, "text/plain", "File not found");
    }
  });

  // Route to take photo
  server.on("/take", HTTP_GET, [](AsyncWebServerRequest *request){
    takePicture();
    request->send(200, "text/html", "<script>alert('Photo taken!'); window.location.href='/';</script>");
  });

  // Route untuk menghapus semua foto
  server.on("/delete-all", HTTP_GET, [](AsyncWebServerRequest *request){
    deleteAllPhotos();
    request->send(200, "text/html", "<script>alert('All photos deleted!'); window.location.href='/';</script>");
  });

  // Start Server
  server.begin();
  
  Serial.println("System ready - Press button to take picture");
}

void loop() { 
 if (digitalRead(BUTTON_PIN) == HIGH) {
   Serial.println("Button pressed - taking picture...");
   takePicture();
   
   delay(200);
 }
  
  delay(100);
}
