#pragma once

// NOTE:
// This file was missing from the repo but is required by `sketch_jan31e.ino`.
// The pin numbers below are placeholders so the project can compile.
// You MUST update them to match your specific Waveshare ESP32-S3 watch wiring.

// Display resolution (CO5300 panels are commonly 466x466, adjust if yours differs)
#ifndef LCD_WIDTH
#define LCD_WIDTH 466
#endif
#ifndef LCD_HEIGHT
#define LCD_HEIGHT 466
#endif

// QSPI display pins (placeholders)
#ifndef LCD_CS
#define LCD_CS 7
#endif
#ifndef LCD_SCLK
#define LCD_SCLK 6
#endif
#ifndef LCD_SDIO0
#define LCD_SDIO0 11
#endif
#ifndef LCD_SDIO1
#define LCD_SDIO1 12
#endif
#ifndef LCD_SDIO2
#define LCD_SDIO2 13
#endif
#ifndef LCD_SDIO3
#define LCD_SDIO3 14
#endif
#ifndef LCD_RESET
#define LCD_RESET 5
#endif

// I2C pins (placeholders)
#ifndef IIC_SDA
#define IIC_SDA 8
#endif
#ifndef IIC_SCL
#define IIC_SCL 9
#endif

// Touch panel pins (placeholders)
#ifndef TP_INT
#define TP_INT 4
#endif
#ifndef TP_RESET
#define TP_RESET 3
#endif
