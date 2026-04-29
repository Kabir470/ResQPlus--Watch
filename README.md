<div align="center">

<img src="https://img.shields.io/badge/ResQ%2B-Watch-red?style=for-the-badge&logo=data:image/svg+xml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9IjAgMCAyNCAyNCI+PHBhdGggZmlsbD0id2hpdGUiIGQ9Ik0xMiAyQzYuNDggMiAyIDYuNDggMiAxMnM0LjQ4IDEwIDEwIDEwIDEwLTQuNDggMTAtMTBTMTcuNTIgMiAxMiAyek0xMiAyMGMtNC40MSAwLTgtMy41OS04LThzMy41OS04IDgtOCA4IDMuNTkgOCA4LTMuNTkgOC04IDh6bTEtMTNoLTJ2NmwyLjI1IDIuMjUgMS40Mi0xLjQyTDEzIDEyLjE3VjdoLTF6Ii8+PC9zdmc+" alt="ResQ+ Watch" />

# 🆘 ResQ+ Watch

### *Your wrist. Your lifeline.*

**A personal-safety wearable powered by ESP32-S3 with a paired React Native app — one tap sends an instant SOS.**

<br/>

[![ESP-IDF](https://img.shields.io/badge/ESP--IDF-v5.x-blue?style=flat-square&logo=espressif&logoColor=white)](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/)
[![React Native](https://img.shields.io/badge/React%20Native-Expo-61DAFB?style=flat-square&logo=react&logoColor=black)](https://expo.dev/)
[![Firebase](https://img.shields.io/badge/Firebase-Realtime%20DB-FFCA28?style=flat-square&logo=firebase&logoColor=black)](https://firebase.google.com/)
[![Bluetooth](https://img.shields.io/badge/Bluetooth-5.0%20LE-0082FC?style=flat-square&logo=bluetooth&logoColor=white)](https://www.bluetooth.com/)
[![LVGL](https://img.shields.io/badge/UI-LVGL%208.x-00B4D8?style=flat-square)](https://lvgl.io/)
[![License](https://img.shields.io/badge/License-MIT-green?style=flat-square)](LICENSE)

<br/>

```
╔══════════════════════════════════════════════╗
║  🔴  PRESS BUTTON  →  📡  BLE ALERT  →  📱  ║
║       on watch           sent instantly    app notified
╚══════════════════════════════════════════════╝
```

</div>

---

## 📋 Table of Contents

- [✨ Features](#-features)
- [🗂️ Repository Structure](#️-repository-structure)
- [🔧 Hardware](#-hardware)
- [🏗️ Architecture](#️-architecture)
- [⚡ Quick Start](#-quick-start)
  - [Watch Firmware](#watch-firmware)
  - [Mobile App](#mobile-app)
- [📡 BLE Protocol](#-ble-protocol)
- [🔥 Firebase Setup](#-firebase-setup)
- [🤝 Contributing](#-contributing)

---

## ✨ Features

<div align="center">

| 🛡️ Safety | 📱 Mobile App | ⌚ Watch Hardware |
|:---:|:---:|:---:|
| One-tap SOS alert | Live dashboard | 2.06" AMOLED display |
| BLE pairing approval | Full alert history | Capacitive touch |
| Incident reporting | Interactive map view | QMI8658 IMU (6-axis) |
| Admin broadcast notices | Community safety feed | Battery monitoring |
| Real-time Firebase sync | User profiles | LVGL touch UI |

</div>

<br/>

> 🔴 **SOS Button** — Press the boot button on the watch to instantly fire a BLE notification to the paired phone, which logs the alert to Firebase and notifies all trusted contacts.

> 🔐 **Secure Pairing** — The watch shows the BD address of any connecting device on-screen and waits for explicit Accept/Reject before any data flows.

> 🗺️ **Situational Awareness** — Geo-tagged alerts appear on a live map so responders know exactly where to go.

---

## 🗂️ Repository Structure

```
ResQPlus--Watch/
│
├── 📁 main/                        ← ESP-IDF firmware (C)
│   ├── 🔧 main.c                   ← App entry, LVGL UI, gyro task
│   ├── 📡 ble_server.c / .h        ← BLE GATT server
│   ├── 🔋 battery_monitor.c / .h   ← ADC battery monitoring task
│   └── 📁 watch_ui/                ← LVGL screens (SquareLine Studio)
│       ├── ui_Screen1.*            ← Watch-face / main screen
│       ├── ui_SettingsScreen.*     ← Settings
│       ├── ui_AboutScreen.*        ← About
│       ├── ui_AppDrawer.*          ← App launcher
│       ├── ui_TimeSettings.*       ← Time configuration
│       └── ui_events.*             ← UI event callbacks
│
├── 📁 application side/            ← React Native / Expo mobile app
│   └── src/
│       ├── 📱 screens/             ← One file per screen (17 screens)
│       ├── 🧭 navigation/          ← React Navigation config
│       ├── 🧩 components/          ← Reusable UI components
│       ├── 🔄 context/             ← React context providers
│       ├── 🛠️  utils/               ← Firebase client, BLE service
│       ├── 🎨 constants/           ← Colours, layout tokens
│       └── 🖼️  assets/              ← Images and fonts
│
├── 📄 CMakeLists.txt               ← ESP-IDF project CMake
├── 📄 sdkconfig / sdkconfig.defaults
├── 📄 partitions.csv               ← Custom flash partition table
├── 📁 project_info/                ← Datasheets & reference docs
└── 📁 esp-watch-firmware/          ← Pre-built firmware binaries
```

---

## 🔧 Hardware

<div align="center">

| Component | Spec |
|:---:|:---|
| 🖥️ **MCU Board** | [Waveshare ESP32-S3-Touch-AMOLED-2.06](https://www.waveshare.com/wiki/ESP32-S3-Touch-AMOLED-2.06) |
| 📺 **Display** | 2.06-inch AMOLED, 240 × 296, capacitive multi-touch |
| 🧭 **IMU** | QMI8658 — 6-axis accelerometer + gyroscope (I²C) |
| 📡 **Wireless** | Wi-Fi 2.4 GHz + Bluetooth 5 LE (built-in) |
| ⚙️ **Framework** | ESP-IDF v5.x (FreeRTOS) |
| 🎨 **UI Library** | LVGL 8.x |

</div>

> 📚 Datasheets, schematics, and reference documents live in [`project_info/`](project_info/).

---

## 🏗️ Architecture

```
                        ╔═══════════════════════════════════╗
                        ║   ⌚  ESP32-S3 Watch               ║
                        ║   ┌──────────┐  ┌──────────────┐  ║
                        ║   │ LVGL UI  │  │  BLE GATT    │  ║
                        ║   │ (touch)  │  │  Server      │  ║
                        ║   └──────────┘  └──────┬───────┘  ║
                        ║   ┌──────────────────┐  │          ║
                        ║   │  Battery Monitor │  │ Notify   ║
                        ║   └──────────────────┘  │          ║
                        ╚════════════════════════ │ ═════════╝
                                                  │
                                    ╔═════════════╧══════════╗
                                    ║  🔵 Bluetooth 5 LE     ║
                                    ╚═════════════╤══════════╝
                                                  │
                        ╔════════════════════════ │ ═════════╗
                        ║   📱  React Native App  │          ║
                        ║   ┌──────────┐  ┌───────┴──────┐  ║
                        ║   │   BLE    │  │   Screens /  │  ║
                        ║   │ Service  │  │   Navigation │  ║
                        ║   └──────────┘  └──────────────┘  ║
                        ║   ┌──────────────────────────────┐ ║
                        ║   │  Firebase Realtime Database  │ ║
                        ║   └──────────────┬───────────────┘ ║
                        ╚══════════════════ │ ════════════════╝
                                           │
                                    ╔══════╧═════════════╗
                                    ║  ☁️  Firebase Cloud  ║
                                    ║  Realtime DB + Auth ║
                                    ╚════════════════════╝
```

---

## ⚡ Quick Start

### Watch Firmware

**Prerequisites:** [ESP-IDF v5.x](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/), CMake ≥ 3.16, USB-C cable

```bash
# 1️⃣  Activate ESP-IDF environment (once per shell session)
. $IDF_PATH/export.sh

# 2️⃣  (Optional) Open menuconfig to tweak settings
idf.py menuconfig

# 3️⃣  Build the firmware
idf.py build

# 4️⃣  Flash to the watch  (replace port as needed)
idf.py -p /dev/ttyUSB0 flash

# 5️⃣  Monitor serial output
idf.py -p /dev/ttyUSB0 monitor
```

> 💡 **Tip:** Hold the **BOOT** button on the Waveshare board when connecting USB to enter download mode if the auto-reset doesn't trigger.

---

### Mobile App

**Prerequisites:** Node.js ≥ 18, Expo CLI, Android/iOS device or emulator

```bash
# 1️⃣  Enter the app folder
cd "application side"

# 2️⃣  Install dependencies
npm install

# 3️⃣  Add your Firebase config  (see Firebase Setup below)

# 4️⃣  Launch Expo dev server
npx expo start

# 5️⃣  Scan the QR with Expo Go  — or press A (Android) / I (iOS)
```

---

## 📡 BLE Protocol

The watch is a **GATT peripheral**; the phone is the **central**.

```
Watch (Server)                          Phone (Client)
──────────────                          ─────────────
  Service: 4fafc201-...
    ├─ TX Characteristic (notify)  ──→  subscribe & receive alerts
    │   beb5483e-...
    └─ RX Characteristic (write)   ←──  send commands
        beb5483f-...
```

| Role | UUID |
|:---|:---|
| **Service** | `4fafc201-1fb5-459e-8fcc-c5c9c331914b` |
| **Alert TX** *(notify)* | `beb5483e-36e1-4688-b7f5-ea07361b26a8` |
| **Command RX** *(write)* | `beb5483f-36e1-4688-b7f5-ea07361b26a8` |

Every button press → firmware increments an internal counter → BLE notification → app logs to Firebase.

🔐 New connections require **manual approval on the watch** (BD address shown on-screen → Accept / Reject).

---

## 🔥 Firebase Setup

1. Create a project at [Firebase Console](https://console.firebase.google.com/)
2. Enable **Realtime Database** and **Authentication → Email/Password**
3. Paste your config into `application side/src/utils/firebase.js`:

```js
const firebaseConfig = {
  apiKey:            "YOUR_API_KEY",
  authDomain:        "YOUR_PROJECT.firebaseapp.com",
  databaseURL:       "https://YOUR_PROJECT-default-rtdb.firebaseio.com",
  projectId:         "YOUR_PROJECT",
  storageBucket:     "YOUR_PROJECT.appspot.com",
  messagingSenderId: "YOUR_SENDER_ID",
  appId:             "YOUR_APP_ID"
};
```

4. Lock down your Realtime Database rules to require authentication:

```json
{
  "rules": {
    ".read": "auth != null",
    ".write": "auth != null"
  }
}
```

---

## 🤝 Contributing

```
Fork → Branch → Code → Test on Hardware → Pull Request
```

1. 🍴 Fork the repo and create a descriptive feature branch
2. 🖊️ Follow the existing style — C for firmware, JavaScript/React Native for the app
3. 🔩 Test firmware changes on real hardware before submitting
4. ✅ Open a PR with a clear description of what changed and why

---

<div align="center">

**Built with ❤️ for personal safety**

*ResQ+ Watch — because every second counts.*

</div>
