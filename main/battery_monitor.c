#include "battery_monitor.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/i2c_master.h"

#include "bsp/esp-bsp.h"
#include "bsp/display.h"

#include "ui.h"

static const char *TAG = "battery";

// AXP2101 (used by the Arduino demo)
#define AXP2101_I2C_ADDR (0x34)
#define AXP2101_REG_STATUS1 (0x00)
#define AXP2101_REG_STATUS2 (0x01)
#define AXP2101_REG_ADC_CHANNEL_CTRL (0x30)
#define AXP2101_REG_ADC_BATT_H (0x34)
#define AXP2101_REG_ADC_BATT_L (0x35)
#define AXP2101_REG_BAT_PERCENT (0xA4)
#define AXP2101_REG_BAT_DET_CTRL (0x68)

static i2c_master_dev_handle_t axp_dev;
static bool axp_ready;

static esp_err_t axp_read(uint8_t reg, void *data, size_t len)
{
    return i2c_master_transmit_receive(axp_dev, &reg, 1, data, len, 100);
}

static esp_err_t axp_write(uint8_t reg, const void *data, size_t len)
{
    uint8_t tmp[1 + 8];
    if (len > 8)
    {
        return ESP_ERR_INVALID_SIZE;
    }
    tmp[0] = reg;
    if (len > 0)
    {
        memcpy(&tmp[1], data, len);
    }
    return i2c_master_transmit(axp_dev, tmp, len + 1, 100);
}

static esp_err_t axp_read_u8(uint8_t reg, uint8_t *out)
{
    return axp_read(reg, out, 1);
}

static esp_err_t axp_write_u8(uint8_t reg, uint8_t val)
{
    return axp_write(reg, &val, 1);
}

static esp_err_t axp_update_bits(uint8_t reg, uint8_t mask, uint8_t value)
{
    uint8_t cur;
    esp_err_t err = axp_read_u8(reg, &cur);
    if (err != ESP_OK)
    {
        return err;
    }
    uint8_t next = (cur & (uint8_t)~mask) | (value & mask);
    if (next == cur)
    {
        return ESP_OK;
    }
    return axp_write_u8(reg, next);
}

static bool axp_is_battery_connected(void)
{
    uint8_t status1;
    if (axp_read_u8(AXP2101_REG_STATUS1, &status1) != ESP_OK)
    {
        return false;
    }
    // STATUS1 bit3: battery present (per common AXP status layout)
    return (status1 & (1u << 3)) != 0;
}

static bool axp_is_charging(void)
{
    uint8_t status2;
    if (axp_read_u8(AXP2101_REG_STATUS2, &status2) != ESP_OK)
    {
        return false;
    }
    // XPowersLib: (STATUS2 >> 5) == 0x01 means charging
    return ((status2 >> 5) & 0x07) == 0x01;
}

static int axp_get_battery_percent(void)
{
    uint8_t pct;
    if (axp_read_u8(AXP2101_REG_BAT_PERCENT, &pct) != ESP_OK)
    {
        return -1;
    }
    if (pct > 100)
    {
        return -1;
    }
    return (int)pct;
}

static int axp_get_battery_mv(void)
{
    uint8_t buf[2];
    if (axp_read(AXP2101_REG_ADC_BATT_H, buf, sizeof(buf)) != ESP_OK)
    {
        return -1;
    }
    // 13-bit ADC value (H:5 bits, L:8 bits)
    uint16_t raw = (uint16_t)((buf[0] & 0x1F) << 8) | buf[1];

    // Datasheet scaling differs by chip; for UI fallback this approximation is fine.
    // Many AXP battery voltage ADCs use ~1.1mV/LSB.
    int mv = (int)((raw * 11u) / 10u);
    return mv;
}

static int voltage_to_percent(int mv)
{
    // Simple LiPo approximation for fallback when fuel-gauge percent isn't available.
    // Clamp to [0,100].
    const int empty_mv = 3300;
    const int full_mv = 4200;
    if (mv <= empty_mv)
        return 0;
    if (mv >= full_mv)
        return 100;
    return (int)(((mv - empty_mv) * 100) / (full_mv - empty_mv));
}

static void battery_ui_set_unknown(void)
{
    if (!bsp_display_lock(50))
    {
        return;
    }

    if (ui_Bar2)
    {
        lv_bar_set_value(ui_Bar2, 0, LV_ANIM_OFF);
    }
    if (ui_batLBL)
    {
        lv_label_set_text(ui_batLBL, "--%");
    }

    bsp_display_unlock();
}

static void battery_ui_set(int pct, bool charging)
{
    (void)charging;

    if (pct < 0)
        pct = 0;
    if (pct > 100)
        pct = 100;

    if (!bsp_display_lock(50))
    {
        return;
    }

    if (ui_Bar2)
    {
        lv_bar_set_value(ui_Bar2, pct, LV_ANIM_OFF);
    }
    if (ui_batLBL)
    {
        char buf[8];
        snprintf(buf, sizeof(buf), "%d%%", pct);
        lv_label_set_text(ui_batLBL, buf);
    }

    bsp_display_unlock();
}

static void battery_task(void *arg)
{
    (void)arg;

    // If the UI hasn't been initialized yet, wait a bit.
    vTaskDelay(pdMS_TO_TICKS(250));

    while (true)
    {
        bool batt_connected = axp_is_battery_connected();
        bool charging = axp_is_charging();

        int pct = -1;
        if (batt_connected)
        {
            pct = axp_get_battery_percent();
            if (pct < 0)
            {
                int mv = axp_get_battery_mv();
                if (mv > 0)
                {
                    pct = voltage_to_percent(mv);
                }
            }
        }

        if (!batt_connected || pct < 0)
        {
            battery_ui_set_unknown();
        }
        else
        {
            battery_ui_set(pct, charging);
        }

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

static esp_err_t axp_init(void)
{
    if (axp_ready)
    {
        return ESP_OK;
    }

    esp_err_t err = bsp_i2c_init();
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "bsp_i2c_init failed: %s", esp_err_to_name(err));
        return err;
    }

    i2c_master_bus_handle_t bus = bsp_i2c_get_handle();
    if (bus == NULL)
    {
        return ESP_ERR_INVALID_STATE;
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = AXP2101_I2C_ADDR,
        .scl_speed_hz = 400000,
    };

    err = i2c_master_bus_add_device(bus, &dev_cfg, &axp_dev);
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "add AXP2101 failed: %s", esp_err_to_name(err));
        return err;
    }

    // Enable battery detection and battery voltage ADC channel (safe no-ops if already enabled).
    (void)axp_update_bits(AXP2101_REG_BAT_DET_CTRL, 1u << 0, 1u << 0);
    (void)axp_update_bits(AXP2101_REG_ADC_CHANNEL_CTRL, 1u << 0, 1u << 0);

    axp_ready = true;
    return ESP_OK;
}

void battery_monitor_start(void)
{
    if (axp_init() != ESP_OK)
    {
        ESP_LOGW(TAG, "AXP2101 not detected/initialized; battery UI will not update");
        return;
    }

    static TaskHandle_t task_handle;
    if (task_handle != NULL)
    {
        return;
    }

    xTaskCreatePinnedToCore(battery_task, "battery_task", 4096, NULL, 4, &task_handle, 1);
}
