#include <Arduino.h>
#include "esp_camera.h"
#include "FS.h"
#include "SD_MMC.h"
#include "FFat.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>

// Cloudinary Configuration
const char* CLOUD_NAME      = "kv80ayfn";
const char* UPLOAD_PRESET   = "await_cam";
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

// System Globals
bool isCameraReady = false;
bool isSDCardReady = false;
bool isOnline      = false;
bool inSetupMode   = false;
int  photoIndex    = 1;

// Wi-Fi Storage & Web Setup Portal
Preferences prefs;
WebServer   setupServer(80);
DNSServer   dnsServer;
String      wifi_ssid = "Main Wifi";
String      wifi_pass = "Tswws4585";

// LED Helpers
void setRGB(uint8_t r, uint8_t g, uint8_t b) {
  neopixelWrite(RGB_LED_PIN, r, g, b);
}

// MicroSD Card Initialization
bool initSDCard() {
  Serial.println("[SD] Initializing MicroSD Card (SD_MMC)...");
  if (!SD_MMC.setPins(SD_MMC_CLK, SD_MMC_CMD, SD_MMC_D0)) {
    Serial.println("[SD] SD_MMC setPins failed.");
    return false;
  }

  if (!SD_MMC.begin("/sdcard", true)) { // 1-bit high speed mode
    Serial.println("[SD] SD Card Mount Failed.");
    return false;
  }

  uint8_t cardType = SD_MMC.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("[SD] No SD Card attached.");
    return false;
  }

  uint64_t totalBytes = SD_MMC.totalBytes();
  Serial.printf("[SD] Mounted! Total: %llu MB\n", totalBytes / (1024 * 1024));

  // Resume photo numbering
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

// Camera OV5640 Initialization with High-Speed 20MHz XCLK
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
  config.xclk_freq_hz = 20000000;          // 20MHz for fast frame rates & zero-lag shutter
  config.frame_size   = FRAMESIZE_UXGA;    // 1600x1200
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode    = CAMERA_GRAB_LATEST; // Always keep the freshest real-time frame
  config.fb_location  = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;
  config.fb_count     = 2;                 // Double buffering in 8MB Octal PSRAM

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("[Camera] Init failed: 0x%x\n", err);
    return false;
  }

  sensor_t *s = esp_camera_sensor_get();
  if (s) {
    s->set_brightness(s, 1);
    s->set_contrast(s, 1);
    s->set_saturation(s, 1);
    s->set_wb_mode(s, 0); // Auto White Balance
  }

  Serial.println("[Camera] OV5640 initialized (UXGA 20MHz Zero-Lag)");
  return true;
}

// Fast Wi-Fi Connection (Non-blocking fallback to Offline Mode in 3.5s)
bool connectToWiFi() {
  if (wifi_ssid.length() == 0) {
    Serial.println("[Wi-Fi] No SSID configured. Entering Offline Mode.");
    isOnline = false;
    return false;
  }

  Serial.printf("\n[Wi-Fi] Connecting to '%s'...\n", wifi_ssid.c_str());
  WiFi.mode(WIFI_STA);
  WiFi.setTxPower(WIFI_POWER_7dBm);
  WiFi.begin(wifi_ssid.c_str(), wifi_pass.c_str());

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 3500) {
    setRGB(15, 10, 0);
    delay(120);
    setRGB(0, 0, 0);
    delay(120);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[Wi-Fi] Connected! IP: " + WiFi.localIP().toString());
    isOnline = true;
    setRGB(0, 25, 0);
    delay(400);
    setRGB(0, 0, 0);
    return true;
  } else {
    Serial.println("\n[Wi-Fi] Offline Mode (No network found). Photos saved to SD Card instantly!");
    isOnline = false;
    setRGB(20, 10, 0);
    delay(300);
    setRGB(0, 0, 0);
    return false;
  }
}

// Upload photo to Cloudinary
bool uploadToCloudinary(camera_fb_t *fb, const char *filename) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[Cloud] Wi-Fi offline. Photo safely stored in SD Card.");
    return false;
  }

  Serial.printf("[Cloud] Uploading %s to %s...\n", filename, CLOUDINARY_HOST);
  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(10000);

  if (!client.connect(CLOUDINARY_HOST, 443)) {
    Serial.println("[Cloud] SSL connection failed!");
    return false;
  }

  setRGB(0, 0, 30); // Blue upload indicator

  String boundary = "----ESP32LaterCamBoundary";
  String head = "--" + boundary + "\r\n"
              + "Content-Disposition: form-data; name=\"upload_preset\"\r\n\r\n"
              + String(UPLOAD_PRESET) + "\r\n"
              + "--" + boundary + "\r\n"
              + "Content-Disposition: form-data; name=\"tags\"\r\n\r\n"
              + "await_cam\r\n"
              + "--" + boundary + "\r\n"
              + "Content-Disposition: form-data; name=\"file\"; filename=\"" + String(filename + 1) + "\"\r\n"
              + "Content-Type: image/jpeg\r\n\r\n";
  String tail = "\r\n--" + boundary + "--\r\n";

  uint32_t totalLen = head.length() + fb->len + tail.length();

  client.printf("POST /v1_1/%s/image/upload HTTP/1.1\r\n", CLOUD_NAME);
  client.printf("Host: %s\r\n", CLOUDINARY_HOST);
  client.println("User-Agent: ESP32-Later-Cam");
  client.printf("Content-Type: multipart/form-data; boundary=%s\r\n", boundary.c_str());
  client.printf("Content-Length: %u\r\n", totalLen);
  client.println("Connection: close\r\n");

  client.print(head);

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
  client.print(tail);

  String responseLine = "";
  bool uploadOk = false;
  unsigned long respWait = millis();

  while (client.connected() && millis() - respWait < 8000) {
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
    Serial.println(">>> [Cloud] Upload SUCCESS! Photo live on Netlify Gallery!");
    for (int i = 0; i < 2; i++) {
      setRGB(0, 40, 0);
      delay(100);
      setRGB(0, 0, 0);
      delay(80);
    }
    return true;
  } else {
    Serial.println("[Cloud] Upload timeout. Photo remains safe on SD Card.");
    setRGB(25, 0, 0);
    delay(250);
    setRGB(0, 0, 0);
    return false;
  }
}

// Instant Shutter Capture
void takePhoto() {
  if (!isCameraReady) {
    Serial.println("[Shutter] Camera not ready!");
    return;
  }

  char filename[32];
  snprintf(filename, sizeof(filename), "/photo_%04d.jpg", photoIndex);
  photoIndex++;

  Serial.printf("\n[Shutter] Snapping photo %s...\n", filename);

  // 1. Shutter Flash: 35ms crisp pulse
  ledcWrite(LED_PWM_CHANNEL, 255);

  // 2. Real-time capture from OV5640
  camera_fb_t *fb = esp_camera_fb_get();

  // 3. Turn flash off immediately
  ledcWrite(LED_PWM_CHANNEL, 0);

  if (!fb) {
    Serial.println("[Error] Failed to capture frame from OV5640.");
    return;
  }

  // 4. Save to MicroSD Card (SD_MMC)
  bool savedLocally = false;
  if (isSDCardReady) {
    File file = SD_MMC.open(filename, FILE_WRITE);
    if (file) {
      file.write(fb->buf, fb->len);
      file.close();
      savedLocally = true;
      Serial.printf("[SD Card] Saved: %s (%u KB)\n", filename, (unsigned int)(fb->len / 1024));
    }
  }

  if (!savedLocally) {
    File file = FFat.open(filename, FILE_WRITE);
    if (file) {
      file.write(fb->buf, fb->len);
      file.close();
      Serial.printf("[FFat] Saved: %s (%u KB)\n", filename, (unsigned int)(fb->len / 1024));
    }
  }

  // 5. Cloud Upload if Online, or Instant Snap confirmation if Offline
  if (isOnline && WiFi.status() == WL_CONNECTED) {
    uploadToCloudinary(fb, filename);
  } else {
    // Instant Offline Confirmation: Snappy Green Blink
    Serial.println("[Offline Snap] Captured instantly to SD Card!");
    setRGB(0, 30, 0);
    delay(120);
    setRGB(0, 0, 0);
  }

  esp_camera_fb_return(fb);
}

// Wi-Fi Setup Portal HTML Page
String getSetupPage() {
  int n = WiFi.scanNetworks();
  String options = "";
  for (int i = 0; i < n; ++i) {
    String ssid = WiFi.SSID(i);
    int rssi = WiFi.RSSI(i);
    options += "<div class='net' onclick=\"document.getElementById('ssid').value='" + ssid + "'\">";
    options += "<span>📶 " + ssid + "</span><small>" + String(rssi) + " dBm</small></div>";
  }
  if (n <= 0) {
    options = "<p style='color:#888;padding:8px;'>ไม่พบ Wi-Fi อัตโนมัติ (สามารถพิมพ์ชื่อเองด้านล่างได้ครับ)</p>";
  }

  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset='UTF-8'>
  <meta name='viewport' content='width=device-width, initial-scale=1.0'>
  <title>Later Cam - Wi-Fi Setup</title>
  <style>
    body { background: #12100e; color: #f4eee1; font-family: -apple-system, sans-serif; padding: 20px; max-width: 420px; margin: 0 auto; }
    h2 { color: #d3733b; text-align: center; margin-bottom: 6px; }
    p.sub { text-align: center; color: #888; font-size: 0.85rem; margin-bottom: 20px; }
    .card { background: #1f1a16; border: 1px solid #382e25; border-radius: 12px; padding: 16px; margin-bottom: 16px; }
    .net { padding: 10px; border-bottom: 1px solid #2e261f; cursor: pointer; display: flex; justify-content: space-between; font-size: 0.9rem; }
    .net:hover { background: #2a221b; }
    label { display: block; font-size: 0.85rem; color: #aaa; margin: 12px 0 4px; }
    input { width: 100%; box-sizing: border-box; padding: 12px; background: #2a221b; border: 1px solid #44372c; color: #fff; border-radius: 8px; font-size: 1rem; }
    button { width: 100%; padding: 14px; background: #d3733b; color: #fff; border: none; border-radius: 8px; font-size: 1rem; font-weight: bold; cursor: pointer; margin-top: 16px; }
    button:hover { background: #e8864e; }
    .btn-exit { background: transparent; border: 1px solid #44372c; color: #aaa; margin-top: 10px; }
  </style>
</head>
<body>
  <h2>📷 Later Cam</h2>
  <p class='sub'>ตั้งค่า Wi-Fi / Hotspot มือถือสำหรับการพกพาไปใช้นอกสถานที่</p>
  <div class='card'>
    <strong>แตะเลือก Wi-Fi ใกล้เคียง / Hotspot มือถือ:</strong>
    <div style='max-height: 180px; overflow-y: auto; margin-top: 10px;'>
)rawliteral";
  html += options;
  html += R"rawliteral(
    </div>
  </div>
  <form action='/save' method='POST' class='card'>
    <label>ชื่อ Wi-Fi (SSID):</label>
    <input type='text' id='ssid' name='ssid' value=')rawliteral";
  html += wifi_ssid;
  html += R"rawliteral(' required placeholder='เช่น iPhone ของคุณ หรือ Wi-Fi บ้าน'>
    <label>รหัสผ่าน (Password):</label>
    <input type='password' name='pass' value=')rawliteral";
  html += wifi_pass;
  html += R"rawliteral(' placeholder='เว้นว่างไว้ถ้าไม่มีรหัสผ่าน'>
    <button type='submit'>💾 บันทึกและเชื่อมต่อ</button>
  </form>
  <form action='/offline' method='POST'>
    <button type='submit' class='btn-exit'>🍃 สลับไปโหมดออฟไลน์ (ถ่ายลง SD Card ทันที)</button>
  </form>
</body>
</html>
)rawliteral";
  return html;
}

// Handlers for Setup Portal
void handleSave() {
  String new_ssid = setupServer.arg("ssid");
  String new_pass = setupServer.arg("pass");

  prefs.begin("later_cam", false);
  prefs.putString("ssid", new_ssid);
  prefs.putString("pass", new_pass);
  prefs.end();

  String html = "<!DOCTYPE html><html><body style='background:#12100e;color:#fff;text-align:center;padding:50px;font-family:sans-serif;'>"
                "<h2>✅ บันทึกสำเร็จ!</h2><p>กล้องกำลังรีสตาร์ตเพื่อเชื่อมต่อ " + new_ssid + " ...</p></body></html>";
  setupServer.send(200, "text/html", html);
  delay(1500);
  ESP.restart();
}

void handleOffline() {
  inSetupMode = false;
  setupServer.stop();
  dnsServer.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  isOnline = false;

  setRGB(20, 15, 0);
  delay(200);
  setRGB(0, 0, 0);
  Serial.println("[Offline Mode] Exited setup. Camera ready for instant SD shooting.");
}

// Activate Wi-Fi Setup SoftAP
void startWiFiPortal() {
  inSetupMode = true;
  Serial.println("\n=================================");
  Serial.println("  [Setup Mode] Starting SoftAP   ");
  Serial.println("=================================");

  // Purple / Magenta LED
  setRGB(30, 0, 30);

  WiFi.disconnect();
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP("Later-Cam-Setup");

  IPAddress apIP(192, 168, 4, 1);
  IPAddress netMsk(255, 255, 255, 0);
  WiFi.softAPConfig(apIP, apIP, netMsk);

  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.start(53, "*", apIP);

  setupServer.on("/", HTTP_GET, []() {
    setupServer.send(200, "text/html", getSetupPage());
  });

  setupServer.on("/save", HTTP_POST, handleSave);
  setupServer.on("/offline", HTTP_POST, handleOffline);

  setupServer.onNotFound([]() {
    setupServer.sendHeader("Location", "http://192.168.4.1/", true);
    setupServer.send(302, "text/plain", "");
  });

  setupServer.begin();
  Serial.println("[Setup Mode] Hotspot Active: 'Later-Cam-Setup'");
  Serial.println("[Setup Mode] Open browser at: http://192.168.4.1");
}

void setup() {
  setCpuFrequencyMhz(160); // 160MHz Eco Clock

  Serial.begin(115200);
  delay(1000);
  Serial.println("\n======================================");
  Serial.println("     Later Cam - Cloud + SD Edition    ");
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

  // Load saved Wi-Fi credentials from Preferences NVS
  prefs.begin("later_cam", false);
  wifi_ssid = prefs.getString("ssid", "Main Wifi");
  wifi_pass = prefs.getString("pass", "Tswws4585");
  prefs.end();

  // Initialize Storage: MicroSD Card & FFat
  isSDCardReady = initSDCard();
  if (!FFat.begin(true)) {
    Serial.println("[FFat] Mount Failed.");
  }

  // Initialize OV5640 Camera
  isCameraReady = initCamera();

  // Connect to Wi-Fi (Quick 3.5s attempt, seamless fallback to Offline Mode)
  connectToWiFi();

  // Ready Status: 3 Cyan blinks if Online, or 2 Amber blinks if Offline
  if (isOnline) {
    for (int i = 0; i < 3; i++) {
      setRGB(0, 20, 20);
      delay(90);
      setRGB(0, 0, 0);
      delay(80);
    }
    Serial.println("\n>>> [System] Online & Ready! Photos upload automatically.");
  } else {
    for (int i = 0; i < 2; i++) {
      setRGB(20, 15, 0);
      delay(90);
      setRGB(0, 0, 0);
      delay(80);
    }
    Serial.println("\n>>> [System] Offline & Ready! Photos save directly to MicroSD Card.");
  }

  Serial.println("[Tip] Hold BOOT button for 3 seconds anytime to open Wi-Fi Setup Portal!");
}

void loop() {
  // If in Wi-Fi Setup Portal Mode
  if (inSetupMode) {
    dnsServer.processNextRequest();
    setupServer.handleClient();

    // Clicking BOOT button exits setup mode back to camera mode
    if (digitalRead(BUTTON_PIN) == LOW) {
      delay(50);
      if (digitalRead(BUTTON_PIN) == LOW) {
        handleOffline();
      }
    }
    delay(10);
    return;
  }

  // Normal Camera Mode
  static int lastStableState = HIGH;
  static int prevRawReading = HIGH;
  static unsigned long lastDebounceTime = 0;
  static unsigned long lastCaptureTime = 0;
  static unsigned long pressStartTime = 0;
  static bool isHolding = false;

  int rawReading = digitalRead(BUTTON_PIN);

  if (rawReading != prevRawReading) {
    lastDebounceTime = millis();
  }
  prevRawReading = rawReading;

  // 25ms Debounce
  if ((millis() - lastDebounceTime) > 25) {
    if (rawReading != lastStableState) {
      lastStableState = rawReading;

      if (lastStableState == LOW) {
        // Button Pressed Down: Instant Shutter Snap!
        pressStartTime = millis();
        isHolding = true;

        if (millis() - lastCaptureTime > 1000) {
          lastCaptureTime = millis();
          takePhoto();
        }
      } else {
        isHolding = false;
      }
    }
  }

  // Long-press detection (> 2.8s) -> Open Wi-Fi Setup Portal
  if (isHolding && (millis() - pressStartTime > 2800)) {
    isHolding = false;
    startWiFiPortal();
  }

  delay(10);
}
