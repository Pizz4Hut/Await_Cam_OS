#include <Arduino.h>
#include "esp_camera.h"
#include "FS.h"
#include "SD_MMC.h"
#include "FFat.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>

// Wi-Fi Credentials for Home Network
const char* WIFI_SSID = "Main Wifi";
const char* WIFI_PASS = "Tswws4585";

// Cloudinary Configuration
const char* CLOUD_NAME    = "kv80ayfn";
const char* UPLOAD_PRESET = "await_cam";
const char* CLOUDINARY_HOST = "api.cloudinary.com";

// Camera Pin Definitions for ESP32-S3 WROOM CAM with OV5640
#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM     15
#define SIOD_GPIO_NUM      4
#define SIOC_GPIO_NUM      5

#define Y9_GPIO_NUM       16
#define Y8_GPIO_NUM       17
#define Y7_GPIO_NUM       18
#define Y6_GPIO_NUM       12
#define Y5_GPIO_NUM       10
#define Y4_GPIO_NUM        8
#define Y3_GPIO_NUM        9
#define Y2_GPIO_NUM       11
#define VSYNC_GPIO_NUM     6
#define HREF_GPIO_NUM      7
#define PCLK_GPIO_NUM     13

// Onboard Hardware Pins
#define BUTTON_PIN         0
#define FLASH_LED_PIN      2
#define RGB_LED_PIN       48
#define LED_PWM_CHANNEL    4

// MicroSD Card Pins (SD_MMC 1-Bit Mode)
#define SD_MMC_CLK        39
#define SD_MMC_CMD        38
#define SD_MMC_D0         40

bool isCameraReady = false;
bool isSDCardReady = false;
int photoIndex = 1;

// LED Indicators
void setRGB(uint8_t r, uint8_t g, uint8_t b) {
  neopixelWrite(RGB_LED_PIN, r, g, b);
}

void triggerShutterFlash() {
  ledcWrite(LED_PWM_CHANNEL, 180); // White Flash LED on GPIO 2
  setRGB(255, 255, 255);
  delay(90);
  ledcWrite(LED_PWM_CHANNEL, 0);   // Shut off immediately
  setRGB(0, 0, 0);
}

// Connect to Home Wi-Fi
void connectToWiFi() {
  Serial.printf("\nConnecting to Wi-Fi '%s'...\n", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);
  delay(100);

  // Set eco power 7dBm
  WiFi.setTxPower(WIFI_POWER_7dBm);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 12000) {
    setRGB(15, 10, 0); // Subtle amber blinking
    delay(200);
    setRGB(0, 0, 0);
    delay(200);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[Wi-Fi] Connected Successfully!");
    Serial.printf("[Wi-Fi] IP Address: %s\n", WiFi.localIP().toString().c_str());
    // Solid green for 1 second
    setRGB(0, 30, 0);
    delay(1000);
    setRGB(0, 0, 0);
  } else {
    Serial.println("\n[Wi-Fi] Failed to connect. Will retry during upload.");
    setRGB(30, 0, 0); // Red warning
    delay(500);
    setRGB(0, 0, 0);
  }
}

// Initialize MicroSD Card
bool initSDCard() {
  Serial.println("Initializing MicroSD Card (SD_MMC)...");
  if (!SD_MMC.setPins(SD_MMC_CLK, SD_MMC_CMD, SD_MMC_D0)) {
    Serial.println("SD_MMC setPins failed.");
    return false;
  }

  if (!SD_MMC.begin("/sdcard", true)) { // 1-bit mode
    Serial.println("SD Card Mount Failed (Please format as FAT32).");
    return false;
  }

  uint8_t cardType = SD_MMC.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("No SD Card attached.");
    return false;
  }

  uint64_t totalBytes = SD_MMC.totalBytes();
  Serial.printf("MicroSD Card Mounted! Total: %llu MB\n", totalBytes / (1024 * 1024));

  // Count existing photos to continue index
  File root = SD_MMC.open("/");
  if (root) {
    File file = root.openNextFile();
    while (file) {
      String name = file.name();
      if (name.startsWith("/photo_") || name.startsWith("photo_")) {
        photoIndex++;
      }
      file = root.openNextFile();
    }
  }
  return true;
}

// Initialize Camera OV5640
bool initCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0       = Y2_GPIO_NUM;
  config.pin_d1       = Y3_GPIO_NUM;
  config.pin_d2       = Y4_GPIO_NUM;
  config.pin_d3       = Y5_GPIO_NUM;
  config.pin_d4       = Y6_GPIO_NUM;
  config.pin_d5       = Y7_GPIO_NUM;
  config.pin_d6       = Y8_GPIO_NUM;
  config.pin_d7       = Y9_GPIO_NUM;
  config.pin_xclk     = XCLK_GPIO_NUM;
  config.pin_pclk     = PCLK_GPIO_NUM;
  config.pin_vsync    = VSYNC_GPIO_NUM;
  config.pin_href     = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn     = PWDN_GPIO_NUM;
  config.pin_reset    = RESET_GPIO_NUM;
  config.xclk_freq_hz = 10000000;
  config.frame_size   = FRAMESIZE_UXGA; // 1600x1200
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode    = CAMERA_GRAB_LATEST; // Always grab the freshest real-time frame
  config.fb_location  = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;
  config.fb_count     = 2; // Double buffering for zero lag

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x\n", err);
    return false;
  }

  sensor_t *s = esp_camera_sensor_get();
  if (s) {
    s->set_brightness(s, 1);
    s->set_contrast(s, 1);
    s->set_saturation(s, 1);
    s->set_wb_mode(s, 0); // Auto WB
  }

  Serial.println("OV5640 Camera initialized successfully (UXGA)");
  return true;
}

// Upload JPEG buffer to Cloudinary
bool uploadToCloudinary(camera_fb_t *fb, const char* filename) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[Cloud] Wi-Fi not connected. Attempting reconnect...");
    WiFi.reconnect();
    unsigned long waitStart = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - waitStart < 5000) {
      delay(200);
    }
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("[Cloud] Wi-Fi unavailable. Photo saved locally.");
      return false;
    }
  }

  Serial.printf("\n[Cloud] Uploading %s (%u KB) to Cloudinary...\n", filename, (unsigned int)(fb->len / 1024));
  setRGB(0, 0, 30); // Blue LED while uploading

  WiFiClientSecure client;
  client.setInsecure(); // Skip certificate checking for speed and memory
  client.setTimeout(15000);

  if (!client.connect(CLOUDINARY_HOST, 443)) {
    Serial.println("[Cloud] Connection to api.cloudinary.com:443 failed!");
    setRGB(30, 0, 0); // Red
    delay(500);
    setRGB(0, 0, 0);
    return false;
  }

  const char* boundary = "----ESP32CloudinaryBoundary7MA4YWxk";
  String head = "--" + String(boundary) + "\r\n" +
                "Content-Disposition: form-data; name=\"upload_preset\"\r\n\r\n" +
                String(UPLOAD_PRESET) + "\r\n" +
                "--" + String(boundary) + "\r\n" +
                "Content-Disposition: form-data; name=\"tags\"\r\n\r\n" +
                "await_cam\r\n" +
                "--" + String(boundary) + "\r\n" +
                "Content-Disposition: form-data; name=\"file\"; filename=\"" + String(filename) + "\"\r\n" +
                "Content-Type: image/jpeg\r\n\r\n";

  String tail = "\r\n--" + String(boundary) + "--\r\n";

  size_t totalLen = head.length() + fb->len + tail.length();

  // Send HTTP Headers
  client.printf("POST /v1_1/%s/image/upload HTTP/1.1\r\n", CLOUD_NAME);
  client.printf("Host: %s\r\n", CLOUDINARY_HOST);
  client.println("User-Agent: ESP32-Await-Cam");
  client.printf("Content-Type: multipart/form-data; boundary=%s\r\n", boundary);
  client.printf("Content-Length: %u\r\n", totalLen);
  client.println("Connection: close\r\n");

  // Send Head
  client.print(head);

  // Send JPEG Binary in Chunks
  uint8_t *buf = fb->buf;
  size_t remaining = fb->len;
  while (remaining > 0) {
    size_t toWrite = remaining > 2048 ? 2048 : remaining;
    size_t written = client.write(buf, toWrite);
    if (written == 0) break;
    buf += written;
    remaining -= written;
    delay(1);
  }

  // Send Tail
  client.print(tail);

  // Read Response
  String responseLine = "";
  bool uploadOk = false;
  unsigned long respWait = millis();

  while (client.connected() && millis() - respWait < 10000) {
    if (client.available()) {
      char c = client.read();
      responseLine += c;
      if (responseLine.indexOf("200 OK") >= 0 || responseLine.indexOf("\"secure_url\"") >= 0) {
        uploadOk = true;
      }
    }
  }
  client.stop();

  if (uploadOk) {
    Serial.println(">>> [Cloud] Upload SUCCESS! Photo is live on Netlify Web Gallery!");
    // Success Blink (Green)
    for (int i = 0; i < 2; i++) {
      setRGB(0, 50, 0);
      delay(120);
      setRGB(0, 0, 0);
      delay(100);
    }
    return true;
  } else {
    Serial.println("[Cloud] Upload failed or timed out.");
    setRGB(30, 0, 0);
    delay(400);
    setRGB(0, 0, 0);
    return false;
  }
}

// Take Photo, Save to SD Card, and Upload to Cloud
void takePhoto() {
  if (!isCameraReady) {
    Serial.println("Camera is not ready!");
    return;
  }

  char filename[32];
  snprintf(filename, sizeof(filename), "/photo_%04d.jpg", photoIndex);
  photoIndex++;

  Serial.printf("\n[Shutter] Capturing photo %s...\n", filename);

  // 1. Flush any stale frame in DMA buffer to ensure 100% live capture
  camera_fb_t *stale_fb = esp_camera_fb_get();
  if (stale_fb) {
    esp_camera_fb_return(stale_fb);
  }

  // 2. Pulse flash during capture
  ledcWrite(LED_PWM_CHANNEL, 255);

  // 3. Capture fresh frame
  camera_fb_t *fb = esp_camera_fb_get();

  // 4. Turn off flash immediately
  ledcWrite(LED_PWM_CHANNEL, 0);

  if (!fb) {
    Serial.println("Error: Failed to capture frame from OV5640.");
    return;
  }

  // 1. Save locally to MicroSD Card (or FFat fallback)
  bool savedLocally = false;
  if (isSDCardReady) {
    File file = SD_MMC.open(filename, FILE_WRITE);
    if (file) {
      file.write(fb->buf, fb->len);
      file.close();
      savedLocally = true;
      Serial.printf("[SD Card] Saved to SD Card: %s (%u KB)\n", filename, (unsigned int)(fb->len / 1024));
    }
  }

  if (!savedLocally) {
    File file = FFat.open(filename, FILE_WRITE);
    if (file) {
      file.write(fb->buf, fb->len);
      file.close();
      Serial.printf("[FFat] Saved to Flash: %s (%u KB)\n", filename, (unsigned int)(fb->len / 1024));
    }
  }

  // 2. Upload to Cloudinary (for Netlify Gallery)
  uploadToCloudinary(fb, filename);

  // Return framebuffer
  esp_camera_fb_return(fb);
}

void setup() {
  // 160MHz Eco Clock
  setCpuFrequencyMhz(160);

  Serial.begin(115200);
  delay(1000);
  Serial.println("\n======================================");
  Serial.println("   Await Camera - Cloud + SD Edition   ");
  Serial.println("======================================");

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // Setup Flash LED on GPIO 2
  ledcSetup(LED_PWM_CHANNEL, 5000, 8);
  ledcAttachPin(FLASH_LED_PIN, LED_PWM_CHANNEL);
  ledcWrite(LED_PWM_CHANNEL, 0);

  // Boot greeting (Amber blink)
  setRGB(20, 15, 0);
  delay(150);
  setRGB(0, 0, 0);

  // Initialize Storage: MicroSD Card & FFat
  isSDCardReady = initSDCard();
  if (!FFat.begin(true)) {
    Serial.println("FFat Mount Failed.");
  }

  // Initialize OV5640 Camera
  isCameraReady = initCamera();

  // Connect to Home Wi-Fi
  connectToWiFi();

  Serial.println("\n[Ready] Press BOOT (GPIO 0) to capture & upload photo to Netlify!");
}

void loop() {
  // Button Debounce & Instant Shutter Trigger
  static int lastStableState = HIGH;
  static int prevRawReading = HIGH;
  static unsigned long lastDebounceTime = 0;
  static unsigned long lastCaptureTime = 0;

  int rawReading = digitalRead(BUTTON_PIN);

  if (rawReading != prevRawReading) {
    lastDebounceTime = millis();
  }
  prevRawReading = rawReading;

  // 30ms Debounce
  if ((millis() - lastDebounceTime) > 30) {
    if (rawReading != lastStableState) {
      lastStableState = rawReading;

      // Instant Shutter: Trigger IMMEDIATELY when button is pressed down (LOW)
      if (lastStableState == LOW) {
        if (millis() - lastCaptureTime > 2500) {
          lastCaptureTime = millis();
          Serial.println("\n[Button] Shutter Clicked (Instant Press)");
          takePhoto();
        }
      }
    }
  }

  delay(10);
}
