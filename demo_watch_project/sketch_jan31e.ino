#include <Wire.h>
#include <Arduino.h>
#include "pin_config.h" 
#include <lvgl.h>
#include "Arduino_GFX_Library.h"
#include "Arduino_DriveBus_Library.h"
#include "XPowersLib.h" 
#include "ui.h" 
#include <esp_now.h>
#include <WiFi.h>
#include "SensorPCF85063.hpp" 
#include "SensorQMI8658.hpp" 

// ==============================================================================
// 🛠️ UI MAPPING
// ==============================================================================
#define UI_LABEL_TIME    ui_Label3      
#define UI_LABEL_SEC     ui_Label6      
#define UI_LABEL_DATE    ui_FontDate    
#define UI_BTN_RESCUE    ui_Button1     
#define UI_MAIN_SCR      ui_Screen1       
#define UI_SETTINGS_SCR  ui_SettingsScreen 

#define UI_ALERT_LBL     ui_lblAlertCount 
#define UI_STEP_LABEL    ui_lblSteps   

#define BOOT_BUTTON_PIN  0                

// --- GLOBAL OBJECTS ---
XPowersAXP2101 power; 
SensorPCF85063 rtc; 
SensorQMI8658 qmi; 

Arduino_DataBus *bus = new Arduino_ESP32QSPI(LCD_CS, LCD_SCLK, LCD_SDIO0, LCD_SDIO1, LCD_SDIO2, LCD_SDIO3);
Arduino_GFX *gfx = new Arduino_CO5300(bus, LCD_RESET, 0, LCD_WIDTH, LCD_HEIGHT, 22, 0, 0, 0);
std::shared_ptr<Arduino_IIC_DriveBus> IIC_Bus = std::make_shared<Arduino_HWIIC>(IIC_SDA, IIC_SCL, &Wire);
void Arduino_IIC_Touch_Interrupt(void);
std::unique_ptr<Arduino_IIC> FT3168(new Arduino_FT3x68(IIC_Bus, FT3168_DEVICE_ADDRESS, DRIVEBUS_DEFAULT_VALUE, TP_INT, Arduino_IIC_Touch_Interrupt));

// --- VARIABLES ---
uint32_t screenWidth, screenHeight, bufSize;
lv_display_t *disp;
lv_color_t *disp_draw_buf;
uint32_t lastTick = 0;
uint32_t lastSensorRead = 0;
bool screenIsOn = true;
int bright = 255; 

// --- COUNTERS ---
int stepCount = 0; 
int alertCount = 0; 

bool stepFlag = false;
float stepThreshold = 12.0;

const char* dayNames[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
const char* monthNames[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};

// ESP-NOW
esp_now_peer_info_t peerInfo;
uint8_t broadcastAddress[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
typedef struct struct_message { char msg[32]; int type; } struct_message;
struct_message myData;

// --- INTERRUPTS ---
void Arduino_IIC_Touch_Interrupt(void) {
  FT3168->IIC_Interrupt_Flag = true;
}
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {}

// --- UI EVENT FUNCTIONS ---

void setBrightness(lv_event_t * e) {
    lv_obj_t * arc = (lv_obj_t *)lv_event_get_target(e); 
    bright = (int)lv_arc_get_value(arc); 
    if (gfx) ((Arduino_CO5300 *)gfx)->setBrightness(bright);
}

void turnScreenOff(lv_event_t * e) {
    screenIsOn = false;
    if (gfx) ((Arduino_CO5300 *)gfx)->setBrightness(0);
    delay(500); 
    FT3168->IIC_Interrupt_Flag = false; 
}

// ALERT FUNCTION
void startRescueSequence(lv_event_t * e) {
    Serial.println(">>> BUTTON PRESSED! <<<"); 
    
    // 1. Send Wireless Alert
    strcpy(myData.msg, "RESQ_PLUS_ALERT");
    myData.type = 1;
    esp_now_send(broadcastAddress, (uint8_t *) &myData, sizeof(myData));

    // 2. Increment Counter
    alertCount++;
    Serial.print("New Count: "); Serial.println(alertCount);

    // 3. Update Label
    if (UI_ALERT_LBL) {
        char countStr[16];
        snprintf(countStr, sizeof(countStr), "%d", alertCount);
        lv_label_set_text(UI_ALERT_LBL, countStr);
        lv_refr_now(disp); 
    }
}

// --- LVGL DRIVERS ---
uint32_t millis_cb(void) { return millis(); }

void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
  gfx->draw16bitRGBBitmap(0, 0, (uint16_t *)px_map, screenWidth, screenHeight);
  lv_disp_flush_ready(disp);
}

void my_touchpad_read(lv_indev_t *indev, lv_indev_data_t *data) {
  if (FT3168->IIC_Interrupt_Flag == true) {
    FT3168->IIC_Interrupt_Flag = false;
    int32_t touchX = FT3168->IIC_Read_Device_Value(FT3168->Arduino_IIC_Touch::Value_Information::TOUCH_COORDINATE_X);
    int32_t touchY = FT3168->IIC_Read_Device_Value(FT3168->Arduino_IIC_Touch::Value_Information::TOUCH_COORDINATE_Y);
    data->state = LV_INDEV_STATE_PR;
    data->point.x = touchX;
    data->point.y = touchY;

    if (!screenIsOn) { 
        screenIsOn = true;
        ((Arduino_CO5300 *)gfx)->setBrightness(bright); 
    }
  } else {
    data->state = LV_INDEV_STATE_REL;
  }
}

// --- SETUP ---
void setup() {
  Serial.begin(115200);
  pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);

  Wire.begin(IIC_SDA, IIC_SCL);
  Wire.setClock(100000); 

  if (!power.begin(Wire, AXP2101_SLAVE_ADDRESS, IIC_SDA, IIC_SCL)) { Serial.println("PMU Failed"); } 
  else {
      power.setALDO1Voltage(3300); power.enableALDO1();
      power.setALDO2Voltage(3300); power.enableALDO2();
      power.setALDO3Voltage(3300); power.enableALDO3();
      power.setBLDO1Voltage(3300); power.enableBLDO1();
      power.enableBattVoltageMeasure();
  }

  rtc.begin(Wire, IIC_SDA, IIC_SCL);
  if (rtc.getDateTime().getYear() < 2026) { rtc.setDateTime(2026, 1, 31, 10, 0, 0); }

  if (!qmi.begin(Wire, QMI8658_L_SLAVE_ADDRESS, IIC_SDA, IIC_SCL)) { Serial.println("QMI Failed"); }
  else {
      qmi.configAccelerometer(SensorQMI8658::ACC_RANGE_4G, SensorQMI8658::ACC_ODR_1000Hz, SensorQMI8658::LPF_MODE_0);
      qmi.enableAccelerometer();
  }

  gfx->begin();
  gfx->fillScreen(BLACK);
  
  pinMode(TP_RESET, OUTPUT);
  digitalWrite(TP_RESET, LOW); delay(20); 
  digitalWrite(TP_RESET, HIGH); delay(200);
  
  if (FT3168->begin()) {
      FT3168->IIC_Write_Device_State(FT3168->Arduino_IIC_Touch::Device::TOUCH_POWER_MODE, FT3168->Arduino_IIC_Touch::Device_Mode::TOUCH_POWER_MONITOR);
  }

  lv_init();
  lv_tick_set_cb(millis_cb);
  screenWidth = gfx->width();
  screenHeight = gfx->height();
  
  bufSize = screenWidth * screenHeight; 
  disp_draw_buf = (lv_color_t *)heap_caps_malloc(bufSize * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
  
  if (!disp_draw_buf) { Serial.println("PSRAM Fail"); while(1); }

  disp = lv_display_create(screenWidth, screenHeight);
  lv_display_set_flush_cb(disp, my_disp_flush);
  lv_display_set_buffers(disp, disp_draw_buf, NULL, bufSize * sizeof(lv_color_t), LV_DISPLAY_RENDER_MODE_DIRECT);

  lv_indev_t *indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, my_touchpad_read);

  WiFi.mode(WIFI_STA);
  esp_now_init();
  esp_now_register_send_cb(OnDataSent);
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0; peerInfo.encrypt = false;
  esp_now_add_peer(&peerInfo);

  ui_init(); 
  ((Arduino_CO5300 *)gfx)->setBrightness(bright);
}

// --- LOOP ---
void loop() {
  lv_task_handler(); 

  // 1. SAFE SENSOR LOOP (50ms Timer)
  if (millis() - lastSensorRead > 50) { 
      lastSensorRead = millis();
      float accX, accY, accZ;
      if (qmi.getDataReady()) {
          if (qmi.getAccelerometer(accX, accY, accZ)) {
              float magnitude = sqrt(accX*accX + accY*accY + accZ*accZ) * 9.8;
              if (magnitude > stepThreshold && !stepFlag) {
                  stepCount++;
                  stepFlag = true;
              } else if (magnitude < 10.0) { stepFlag = false; }
          }
      }
  }

  // 2. UI UPDATES (1 Second Timer)
  if (millis() - lastTick > 1000) {
      lastTick = millis();
      RTC_DateTime datetime = rtc.getDateTime();
      
      char timeBuf[8]; snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", datetime.getHour(), datetime.getMinute());
      if(UI_LABEL_TIME) lv_label_set_text(UI_LABEL_TIME, timeBuf);
      
      #ifdef UI_STEP_LABEL
      if (UI_STEP_LABEL) {
          char stepStr[10]; snprintf(stepStr, sizeof(stepStr), "%d", stepCount);
          lv_label_set_text(UI_STEP_LABEL, stepStr);
      }
      #endif
  }

  // 3. PHYSICAL BUTTON (RESET ALERT COUNTER)
  if (digitalRead(BOOT_BUTTON_PIN) == LOW) {
      delay(50); // Debounce
      if (digitalRead(BOOT_BUTTON_PIN) == LOW) {
          // --- RESET LOGIC ---
          Serial.println("Resetting Alert Count...");
          alertCount = 0;
          
          if (UI_ALERT_LBL) {
               lv_label_set_text(UI_ALERT_LBL, "0");
               lv_refr_now(disp); // Force update screen immediately
          }
          
          // Wait until button is released (so it doesn't loop)
          while(digitalRead(BOOT_BUTTON_PIN) == LOW) { delay(10); } 
      }
  }
  delay(10); 
}