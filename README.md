# Await Camera OS 📷☁️

**DIY Vintage Analog Emulation Camera powered by ESP32-S3 (N16R8), OV5640 Sensor & Cloud Integration.**

---

## 🌟 Features
- **Sensor**: OV5640 5MP camera capturing high-resolution UXGA (1600x1200) JPEGs.
- **Local Storage**: MicroSD Card support via `SD_MMC` 1-bit high-speed interface (with FFat Flash fallback).
- **Cloud Auto-Upload**: Automatically uploads captured photos over Home Wi-Fi to Cloudinary using HTTPS.
- **Netlify Web Gallery**: Live responsive retro film gallery with vintage film CSS filters, lightbox view, and direct download links.
- **Physical Shutter**: BOOT button (GPIO 0) with 90ms shutter flash and RGB status feedback.

---

## 🚀 Netlify Deployment
This repository is configured to deploy directly to [Netlify](https://www.netlify.com/):
1. Log in to Netlify.
2. Click **"Add new site"** > **"Import an existing project"** > **GitHub**.
3. Select this repository: `Pizz4Hut/Await_Cam_OS`.
4. Leave build command empty and publish directory as `/` (Root).
5. Click **Deploy Await_Cam_OS**!

---

## ⚙️ Cloudinary Setup
1. Log in to [Cloudinary Console](https://console.cloudinary.com).
2. Go to **Settings ⚙️** > **Security**.
3. Under **Restricted media types**, uncheck **Resource list** and click **Save**.
4. Photos uploaded with tag `await_cam` will automatically appear in your Netlify gallery!

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
