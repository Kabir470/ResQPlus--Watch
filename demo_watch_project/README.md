# ESP32-S3 Watch (VS Code setup)

This repo currently contains an Arduino-style sketch (`sketch_jan31e.ino`).
VS Code works best with this style via **PlatformIO**.

## Open in VS Code

1. Install the recommended extension: **PlatformIO IDE**.
2. Open this folder in VS Code.
3. Use PlatformIO commands:
   - Build: `PlatformIO: Build`
   - Upload: `PlatformIO: Upload`
   - Serial monitor: `PlatformIO: Serial Monitor`

## Notes

- Your sketch includes `pin_config.h`, but it is not present in this folder. You’ll need to add it (commonly under `include/pin_config.h`).
- `platformio.ini` currently uses `board = esp32-s3-devkitc-1`. If your Waveshare board is different, change it.
- The existing `CMakeLists.txt` in the root is not an ESP-IDF project CMake file; it won’t be used by PlatformIO.
