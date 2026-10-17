# Later Cam OS 📷☁️

**DIY Vintage Analog Emulation Camera powered by ESP32-S3 (N16R8), OV5640 Sensor & Cloud Integration.**

---

## 🌟 Features
- **Sensor**: OV5640 5MP camera capturing high-resolution UXGA (1600x1200) JPEGs.
- **Local Storage**: MicroSD Card support via `SD_MMC` 1-bit high-speed interface (with FFat Flash fallback).
- **Cloud Auto-Upload**: Automatically uploads captured photos over Home Wi-Fi to Cloudinary using HTTPS.
- **Netlify Web Gallery**: Live responsive retro film gallery with vintage film CSS filters, lightbox view, and direct download links.
- **Physical Shutter**: BOOT button (GPIO 0) with 80ms shutter flash and RGB status feedback.

---

## 🚀 Netlify Deployment
This repository is configured to deploy directly to [Netlify](https://www.netlify.com/):
1. Log in to Netlify.
2. Click **"Add new site"** > **"Import an existing project"** > **GitHub**.
3. Select this repository: `Pizz4Hut/Await_Cam_OS` (or updated repo name).
4. Leave build command empty and publish directory as `/` (Root).
5. Click **Deploy**!

---

## ⚙️ Cloudinary Setup
1. Log in to [Cloudinary Console](https://console.cloudinary.com).
2. Go to **Settings ⚙️** > **Security**.
3. Under **Restricted media types**, uncheck **Resource list** and click **Save**.
4. Photos uploaded with tag `await_cam` will automatically appear in your Netlify gallery!

---

## 💡 LED Status Signals & Guide
| Status LED (RGB) | Meaning | Description |
|---|---|---|
| 🟠 **Amber Blink (1x)** | Booting | System initializing (mounting SD Card & OV5640) |
| 🩵 **Cyan Blink (3x)** | Online Ready | Wi-Fi connected! Photos will upload to Netlify Gallery |
| 🟠 **Amber Blink (2x)** | Offline Ready | No Wi-Fi found. Camera ready to shoot directly to SD Card instantly! |
| 🟣 **Solid Magenta / Purple** | Wi-Fi Setup Portal | AP `Later-Cam-Setup` active. Connect with phone to configure Wi-Fi |
| ⚡ **White Flash (35ms)** | Shutter Flash | Crisp exposure flash pulse (ultra-fast zero-lag capture) |
| 🔵 **Solid Blue** | Cloud Uploading | Saving to SD Card & uploading via HTTPS to Cloudinary |
| 🟢 **Green Blink (2x)** | Snap / Upload Done | Photo secured and ready for next shot! |
| 🔴 **Red Blink (1x)** | Upload Failed | Network issue; photo remains safely stored on SD Card |

---

## 📸 Shooting & Outdoor Setup Tutorial
1. **Instant Shutter**: Press BOOT button (GPIO 0).
   - High-speed 20MHz XCLK OV5640 capture.
   - Saves to MicroSD Card in under 40ms.
   - If Online: Uploads to Cloudinary automatically.
   - If Offline: Bypasses upload with snappy green blink — zero freeze!
2. **Outdoor Wi-Fi Setup (Phone Hotspot)**:
   - **Hold BOOT button for 3 seconds** anytime.
   - LED turns **🟣 Purple/Magenta**.
   - On your smartphone, connect to Wi-Fi: **`Later-Cam-Setup`**.
   - Open browser to **`http://192.168.4.1`**.
   - Tap your phone hotspot or enter new Wi-Fi name & password -> Click **Save**!
   - Camera restarts and connects to your hotspot automatically.

---

## 🛠️ Hardware Pinout (ESP32-S3 WROOM CAM)
| Function | GPIO |
|---|---|
| Camera XCLK | GPIO 15 |
| Camera SIOD / SIOC | GPIO 4 / GPIO 5 |
| Camera D0 - D7 | GPIO 11, 9, 8, 10, 12, 18, 17, 16 |
| Camera VSYNC / HREF / PCLK | GPIO 6 / GPIO 7 / GPIO 13 |
| MicroSD CLK / CMD / D0 | GPIO 39 / GPIO 38 / GPIO 40 |
| Shutter Button | GPIO 0 (BOOT) |
| Flash LED | GPIO 2 |
| WS2812 RGB Status LED | GPIO 48 |

---

## 📦 Hardware Bill of Materials (BOM) & 3D Print Build
Looking to build your own standalone handheld body with battery & 3D printed enclosure?
Check out the full [Hardware BOM & Buying Guide](file:///c:/Users/hahah/Documents/ESP32%20Retro%20Cam/HARDWARE_BOM.md) with Shopee/Thai electronics search keywords and wiring diagrams!
