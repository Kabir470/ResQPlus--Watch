#pragma once

// Minimal LVGL configuration for this project.
// LVGL will apply defaults for anything not defined here.

// SquareLine project expects 16-bit color.
#define LV_COLOR_DEPTH 16

// Use Arduino millis() as the LVGL tick source.
#define LV_TICK_CUSTOM 1
#define LV_TICK_CUSTOM_INCLUDE "Arduino.h"
#define LV_TICK_CUSTOM_SYS_TIME_EXPR (millis())
