#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_err.h"
#include "lvgl.h"
#include "bsp/esp-bsp.h"
#include "bsp/display.h"
#include "driver/gpio.h"

#include "ui.h"
#include "ble_server.h"

#include "battery_monitor.h"

#include "qmi8658.h"
i2c_master_bus_handle_t bus_handle;
static const char *TAG = "gyro_shapes";

/* Forward declaration needed before any use (avoids implicit-function-declaration). */
static void resqplus_update_count_label(void);

typedef enum
{
    SHAPE_CIRCLE,
    SHAPE_SQUARE,
    SHAPE_TRIANGLE,
    SHAPE_HEXAGON,
    SHAPE_COUNT
} ShapeType;

typedef struct
{
    lv_obj_t *obj;
    ShapeType type;
    int radius;
    int x_pos;
    int y_pos;
    lv_color_t color;
} Shape;

#define MAX_SHAPES 15
#define MIN_SHAPE_SIZE 15
#define MAX_SHAPE_SIZE 30
#define ACCEL_SCALE_FACTOR 5
#define TASK_DELAY_MS 20
#define CALIBRATION_DEADZONE 0.05f

#define SCREEN_WIDTH_MM 33.09f
#define SCREEN_HEIGHT_MM 41.51f
#define CORNER_RADIUS_MM 9.2f

static float accel_bias_x = 0.0f;
static float accel_bias_y = 0.0f;
static bool calibration_done = false;
static bool recalibration_requested = false;
static int display_width = 0;
static int display_height = 0;
static Shape shapes[MAX_SHAPES];
static int shape_count = 0;

lv_color_t get_random_color()
{
    return lv_color_hsv_to_rgb(rand() % 360, 70, 90);
}

bool check_overlap(const Shape *a, const Shape *b)
{
    int dx = a->x_pos - b->x_pos;
    int dy = a->y_pos - b->y_pos;
    int distance_squared = dx * dx + dy * dy;
    int min_distance = a->radius + b->radius;
    return distance_squared < (min_distance * min_distance);
}

lv_obj_t *create_shape_obj(ShapeType type, int size, lv_color_t color)
{
    lv_obj_t *obj;

    switch (type)
    {
    case SHAPE_CIRCLE:
    {
        obj = lv_obj_create(lv_screen_active());
        lv_obj_set_size(obj, size * 2, size * 2);
        lv_obj_set_style_radius(obj, LV_RADIUS_CIRCLE, 0);
        break;
    }
    case SHAPE_SQUARE:
    {
        obj = lv_obj_create(lv_screen_active());
        lv_obj_set_size(obj, size * 2, size * 2);
        lv_obj_set_style_radius(obj, 0, 0);
        break;
    }
    case SHAPE_TRIANGLE:
    {
        obj = lv_obj_create(lv_screen_active());
        lv_obj_set_size(obj, size * 2, size * 2);
        lv_obj_set_style_radius(obj, 0, 0);
        static lv_style_t style;
        lv_style_init(&style);
        lv_style_set_bg_color(&style, color);
        lv_obj_add_style(obj, &style, 0);
        break;
    }
    case SHAPE_HEXAGON:
    {
        obj = lv_obj_create(lv_screen_active());
        lv_obj_set_size(obj, size * 2, size * 2);
        lv_obj_set_style_radius(obj, LV_RADIUS_CIRCLE, 0);
        break;
    }
    default:
        return NULL;
    }

    lv_obj_set_style_bg_color(obj, color, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_shadow_width(obj, 3, 0);
    return obj;
}

void generate_random_shapes()
{
    srand(time(NULL));
    shape_count = 0;

    while (shape_count < MAX_SHAPES)
    {
        Shape new_shape;
        new_shape.type = rand() % SHAPE_COUNT;
        new_shape.radius = MIN_SHAPE_SIZE + rand() % (MAX_SHAPE_SIZE - MIN_SHAPE_SIZE + 1);
        new_shape.color = get_random_color();

        new_shape.obj = create_shape_obj(new_shape.type, new_shape.radius, new_shape.color);
        if (!new_shape.obj)
            continue;

        bool valid_position = false;
        int attempts = 0;

        while (!valid_position && attempts < 100)
        {
            new_shape.x_pos = new_shape.radius + rand() % (display_width - 2 * new_shape.radius);
            new_shape.y_pos = new_shape.radius + rand() % (display_height - 2 * new_shape.radius);

            valid_position = true;
            for (int i = 0; i < shape_count; i++)
            {
                if (check_overlap(&new_shape, &shapes[i]))
                {
                    valid_position = false;
                    break;
                }
            }
            attempts++;
        }

        if (valid_position)
        {
            lv_obj_set_pos(new_shape.obj, new_shape.x_pos - new_shape.radius,
                           new_shape.y_pos - new_shape.radius);
            shapes[shape_count] = new_shape;
            shape_count++;
        }
        else
        {
            lv_obj_del(new_shape.obj);
        }
    }
}

void perform_level_calibration(qmi8658_dev_t *dev)
{
    qmi8658_data_t data;
    const int CALIB_SAMPLES = 200;
    float sum_x = 0.0f, sum_y = 0.0f;
    float max_x = -10.0f, min_x = 10.0f;
    float max_y = -10.0f, min_y = 10.0f;

    ESP_LOGI(TAG, "Starting level calibration...");
    ESP_LOGI(TAG, "Please place device on a level surface");

    for (int i = 0; i < CALIB_SAMPLES; i++)
    {
        if (qmi8658_read_sensor_data(dev, &data) == ESP_OK)
        {
            sum_x += data.accelX;
            sum_y += data.accelY;

            if (data.accelX > max_x)
                max_x = data.accelX;
            if (data.accelX < min_x)
                min_x = data.accelX;
            if (data.accelY > max_y)
                max_y = data.accelY;
            if (data.accelY < min_y)
                min_y = data.accelY;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    float range_x = max_x - min_x;
    float range_y = max_y - min_y;

    if (range_x > 0.1f || range_y > 0.1f)
    {
        ESP_LOGW(TAG, "Calibration unstable (X range: %.4f, Y range: %.4f). Retrying...",
                 range_x, range_y);
        perform_level_calibration(dev);
        return;
    }

    accel_bias_x = sum_x / CALIB_SAMPLES;
    accel_bias_y = sum_y / CALIB_SAMPLES;

    calibration_done = true;
    ESP_LOGI(TAG, "Calibration complete. Bias X: %.4f m/s², Bias Y: %.4f m/s²",
             accel_bias_x, accel_bias_y);
    ESP_LOGI(TAG, "Device is now level. Shapes should be stationary.");
}

void apply_calibration_and_deadzone(qmi8658_data_t *data)
{
    if (calibration_done)
    {
        data->accelX -= accel_bias_x;
        data->accelY -= accel_bias_y;

        if (fabsf(data->accelX) < CALIBRATION_DEADZONE)
            data->accelX = 0.0f;
        if (fabsf(data->accelY) < CALIBRATION_DEADZONE)
            data->accelY = 0.0f;
    }
}

static void __attribute__((unused)) constrain_to_rounded_rect(int *x, int *y, int width, int height,
                                                              float corner_radius_mm, int shape_radius)
{
    float px_per_mm_x = (float)width / SCREEN_WIDTH_MM;
    float px_per_mm_y = (float)height / SCREEN_HEIGHT_MM;

    float px_per_mm = (px_per_mm_x < px_per_mm_y) ? px_per_mm_x : px_per_mm_y;

    int corner_radius_px = (int)(corner_radius_mm * px_per_mm);

    if (corner_radius_px < shape_radius)
    {
        corner_radius_px = shape_radius + 5;
    }

    int safe_left = corner_radius_px;
    int safe_right = width - corner_radius_px;
    int safe_top = corner_radius_px;
    int safe_bottom = height - corner_radius_px;

    if (*x < shape_radius)
        *x = shape_radius;
    if (*x > width - shape_radius)
        *x = width - shape_radius;
    if (*y < shape_radius)
        *y = shape_radius;
    if (*y > height - shape_radius)
        *y = height - shape_radius;

    int corner_radius_effective = corner_radius_px - shape_radius;

    if (*x < safe_left && *y < safe_top)
    {
        float dx = safe_left - *x;
        float dy = safe_top - *y;
        float dist = sqrtf(dx * dx + dy * dy);

        if (dist > corner_radius_effective)
        {
            float ratio = corner_radius_effective / dist;
            *x = safe_left - (int)(dx * ratio);
            *y = safe_top - (int)(dy * ratio);
        }
    }
    else if (*x > safe_right && *y < safe_top)
    {
        float dx = *x - safe_right;
        float dy = safe_top - *y;
        float dist = sqrtf(dx * dx + dy * dy);

        if (dist > corner_radius_effective)
        {
            float ratio = corner_radius_effective / dist;
            *x = safe_right + (int)(dx * ratio);
            *y = safe_top - (int)(dy * ratio);
        }
    }
    else if (*x < safe_left && *y > safe_bottom)
    {
        float dx = safe_left - *x;
        float dy = *y - safe_bottom;
        float dist = sqrtf(dx * dx + dy * dy);

        if (dist > corner_radius_effective)
        {
            float ratio = corner_radius_effective / dist;
            *x = safe_left - (int)(dx * ratio);
            *y = safe_bottom + (int)(dy * ratio);
        }
    }
    else if (*x > safe_right && *y > safe_bottom)
    {
        float dx = *x - safe_right;
        float dy = *y - safe_bottom;
        float dist = sqrtf(dx * dx + dy * dy);

        if (dist > corner_radius_effective)
        {
            float ratio = corner_radius_effective / dist;
            *x = safe_right + (int)(dx * ratio);
            *y = safe_bottom + (int)(dy * ratio);
        }
    }
}

bool check_collision_impending(const Shape *a, const Shape *b, int a_new_x, int a_new_y)
{
    int dx = a_new_x - b->x_pos;
    int dy = a_new_y - b->y_pos;
    int distance_squared = dx * dx + dy * dy;
    int min_distance = a->radius + b->radius;
    return distance_squared < (min_distance * min_distance);
}

void handle_shape_collisions(int index)
{
    for (int i = 0; i < shape_count; i++)
    {
        if (i == index)
            continue;

        if (check_overlap(&shapes[index], &shapes[i]))
        {
            int dx = shapes[index].x_pos - shapes[i].x_pos;
            int dy = shapes[index].y_pos - shapes[i].y_pos;
            float distance = sqrtf(dx * dx + dy * dy);
            float overlap = (shapes[index].radius + shapes[i].radius) - distance;

            if (distance > 0)
            {
                float ratio = overlap / (2 * distance);
                shapes[index].x_pos += (int)(dx * ratio);
                shapes[index].y_pos += (int)(dy * ratio);
                shapes[i].x_pos -= (int)(dx * ratio);
                shapes[i].y_pos -= (int)(dy * ratio);
            }
            else
            {
                shapes[index].x_pos += (rand() % 11) - 5;
                shapes[index].y_pos += (rand() % 11) - 5;
            }
        }
    }
}

static void __attribute__((unused)) shapes_update_task(void *arg)
{
    qmi8658_dev_t *dev = (qmi8658_dev_t *)arg;
    qmi8658_data_t data;

    while (display_width == 0 || display_height == 0)
    {
        display_width = lv_disp_get_hor_res(NULL);
        display_height = lv_disp_get_ver_res(NULL);
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    generate_random_shapes();

    while (1)
    {
        if (recalibration_requested)
        {
            recalibration_requested = false;
            perform_level_calibration(dev);
        }

        bool ready;
        esp_err_t ret = qmi8658_is_data_ready(dev, &ready);
        if (ret != ESP_OK)
        {
            vTaskDelay(pdMS_TO_TICKS(TASK_DELAY_MS));
            continue;
        }

        if (ready)
        {
            ret = qmi8658_read_sensor_data(dev, &data);
            if (ret == ESP_OK)
            {
                apply_calibration_and_deadzone(&data);

                int move_x = -(int)(data.accelY * ACCEL_SCALE_FACTOR);
                int move_y = (int)(data.accelX * ACCEL_SCALE_FACTOR);

                bsp_display_lock(pdMS_TO_TICKS(100));

                for (int i = 0; i < shape_count; i++)
                {
                    int new_x = shapes[i].x_pos + move_x;
                    int new_y = shapes[i].y_pos + move_y;

                    bool collision = false;
                    for (int j = 0; j < shape_count; j++)
                    {
                        if (i == j)
                            continue;
                        if (check_collision_impending(&shapes[i], &shapes[j], new_x, new_y))
                        {
                            collision = true;
                            break;
                        }
                    }

                    if (!collision)
                    {
                        shapes[i].x_pos = new_x;
                        shapes[i].y_pos = new_y;
                    }

                    constrain_to_rounded_rect(&shapes[i].x_pos, &shapes[i].y_pos,
                                              display_width, display_height,
                                              CORNER_RADIUS_MM, shapes[i].radius);

                    handle_shape_collisions(i);

                    lv_obj_set_pos(shapes[i].obj,
                                   shapes[i].x_pos - shapes[i].radius,
                                   shapes[i].y_pos - shapes[i].radius);
                }

                bsp_display_unlock();
            }
        }
        vTaskDelay(pdMS_TO_TICKS(TASK_DELAY_MS));
    }
}

static void __attribute__((unused)) IRAM_ATTR button_isr_handler(void *arg)
{
    recalibration_requested = true;
}

void init_calibration_button()
{
    const int CALIB_BUTTON_GPIO = GPIO_NUM_0;

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << CALIB_BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE};
    gpio_config(&io_conf);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(CALIB_BUTTON_GPIO, button_isr_handler, NULL);
}

static lv_obj_t *resqplus_count_label;
static int resqplus_press_count;
static TaskHandle_t resqplus_boot_btn_task_handle;
static TaskHandle_t resqplus_pwr_btn_task_handle;
static volatile bool resqplus_display_on = true;

/* Forward declarations (avoid implicit function declaration warnings/errors). */
static void resqplus_update_count_label(void);
static void resqplus_boot_btn_task(void *arg);
static void IRAM_ATTR resqplus_boot_btn_isr_handler(void *arg);
static void resqplus_init_boot_button(void);
static void resqplus_pwr_btn_task(void *arg);
static void IRAM_ATTR resqplus_pwr_btn_isr_handler(void *arg);
static void resqplus_init_pwr_button(void);
static void resqplus_display_set(bool on);
static void resqplus_activity(void);
static void resqplus_inc_btn_event_cb(lv_event_t *e);
static void resqplus_reset_btn_event_cb(lv_event_t *e);
static void resqplus_screen_event_cb(lv_event_t *e);
static void resqplus_create_test_screen(void);

/* ================================================================
 *  Clock Update Timer – updates time & date labels every second
 *  Also handles screen timeout / wake.
 * ================================================================ */
static void clock_update_timer_cb(lv_timer_t *timer)
{
    (void)timer;

    /* ── Screen timeout / wake logic ───────────────────────────────────── */
    if (isScreenOff())
    {
        /* Screen is off – check if user touched to wake */
        if (canWakeScreen() && lv_display_get_inactive_time(NULL) < 1000)
        {
            wakeScreen(40); /* restore to 40 % brightness */
        }
        return; /* skip UI updates while screen is off */
    }
    else
    {
        int timeout_sec = getScreenTimeoutSeconds();
        if (timeout_sec > 0)
        {
            uint32_t inactive_ms = lv_display_get_inactive_time(NULL);
            if (inactive_ms > (uint32_t)timeout_sec * 1000)
            {
                setScreenOff(true);
                return;
            }
        }
    }

    /* ── Clock update ──────────────────────────────────────────────────── */

    time_t now;
    time(&now);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    /* Only update if we have a valid time (year >= 2024) */
    if (timeinfo.tm_year + 1900 < 2024)
    {
        /* Show a "--:--" placeholder so user knows time isn't synced */
        if (ui_Label3)
        {
            lv_label_set_text(ui_Label3, "--:--");
        }
        if (ui_FontDate)
        {
            lv_label_set_text(ui_FontDate, "NO TIME");
        }
        return;
    }

    /* Update time label (HH:MM) */
    if (ui_Label3)
    {
        char time_buf[8];
        strftime(time_buf, sizeof(time_buf), "%H:%M", &timeinfo);
        lv_label_set_text(ui_Label3, time_buf);
    }

    /* Update date label (e.g. "SAT, MAR 01") */
    if (ui_FontDate)
    {
        char date_buf[24];
        strftime(date_buf, sizeof(date_buf), "%a, %b %d", &timeinfo);
        /* Convert to uppercase */
        for (int i = 0; date_buf[i]; i++)
        {
            if (date_buf[i] >= 'a' && date_buf[i] <= 'z')
                date_buf[i] -= 32;
        }
        lv_label_set_text(ui_FontDate, date_buf);
    }
}

/* ================================================================
 *  BLE Connection Prompt (overlay on lv_layer_top)
 * ================================================================ */
static lv_obj_t *ble_prompt_overlay = NULL;
static int ble_prompt_ticks = 0;
#define BLE_PROMPT_TIMEOUT 30 /* 30 × 500 ms = 15 s */

static void ble_prompt_dismiss(void)
{
    if (ble_prompt_overlay)
    {
        lv_obj_del(ble_prompt_overlay);
        ble_prompt_overlay = NULL;
    }
    ble_prompt_ticks = 0;
}

static void ble_prompt_accept_cb(lv_event_t *e)
{
    (void)e;
    ble_server_accept_connection();
    ble_prompt_dismiss();
}

static void ble_prompt_reject_cb(lv_event_t *e)
{
    (void)e;
    ble_server_reject_connection();
    ble_prompt_dismiss();
}

static void ble_prompt_timer_cb(lv_timer_t *timer)
{
    (void)timer;

    /* Update BT connected icon on main screen */
    if (ui_lblBtStatus)
    {
        if (ble_server_is_connected())
        {
            lv_obj_remove_flag(ui_lblBtStatus, LV_OBJ_FLAG_HIDDEN);
        }
        else
        {
            lv_obj_add_flag(ui_lblBtStatus, LV_OBJ_FLAG_HIDDEN);
        }
    }

    /* If prompt is already showing, handle timeout */
    if (ble_prompt_overlay)
    {
        ble_prompt_ticks++;
        if (ble_prompt_ticks >= BLE_PROMPT_TIMEOUT)
        {
            ble_server_reject_connection();
            ble_prompt_dismiss();
        }
        return;
    }

    if (!ble_server_has_pending_connection())
    {
        return;
    }

    uint8_t bda[6];
    ble_server_get_pending_bda(bda);

    /* Dark overlay panel on the top layer (visible on any screen) */
    ble_prompt_overlay = lv_obj_create(lv_layer_top());
    lv_obj_set_size(ble_prompt_overlay, 300, 260);
    lv_obj_center(ble_prompt_overlay);
    lv_obj_remove_flag(ble_prompt_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ble_prompt_overlay, lv_color_hex(0x101024), 0);
    lv_obj_set_style_bg_opa(ble_prompt_overlay, 245, 0);
    lv_obj_set_style_radius(ble_prompt_overlay, 20, 0);
    lv_obj_set_style_border_color(ble_prompt_overlay, lv_color_hex(0x0066FF), 0);
    lv_obj_set_style_border_width(ble_prompt_overlay, 2, 0);
    lv_obj_set_style_pad_all(ble_prompt_overlay, 15, 0);

    /* Title */
    lv_obj_t *title = lv_label_create(ble_prompt_overlay);
    lv_label_set_text(title, LV_SYMBOL_BLUETOOTH " Pair Request");
    lv_obj_set_style_text_color(title, lv_color_hex(0x4488FF), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_22, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 5);

    /* Device address */
    lv_obj_t *addr = lv_label_create(ble_prompt_overlay);
    char buf[80];
    snprintf(buf, sizeof(buf),
             "%02X:%02X:%02X:%02X:%02X:%02X\nwants to connect",
             bda[0], bda[1], bda[2], bda[3], bda[4], bda[5]);
    lv_label_set_text(addr, buf);
    lv_obj_set_style_text_color(addr, lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_style_text_align(addr, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(addr, &lv_font_montserrat_16, 0);
    lv_obj_align(addr, LV_ALIGN_CENTER, 0, -15);

    /* Accept button */
    lv_obj_t *acc = lv_button_create(ble_prompt_overlay);
    lv_obj_set_size(acc, 115, 48);
    lv_obj_align(acc, LV_ALIGN_BOTTOM_LEFT, 5, -5);
    lv_obj_set_style_bg_color(acc, lv_color_hex(0x00AA44), 0);
    lv_obj_set_style_radius(acc, 24, 0);
    lv_obj_add_event_cb(acc, ble_prompt_accept_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *acc_lbl = lv_label_create(acc);
    lv_label_set_text(acc_lbl, "Accept");
    lv_obj_set_style_text_color(acc_lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(acc_lbl, &lv_font_montserrat_16, 0);
    lv_obj_center(acc_lbl);

    /* Reject button */
    lv_obj_t *rej = lv_button_create(ble_prompt_overlay);
    lv_obj_set_size(rej, 115, 48);
    lv_obj_align(rej, LV_ALIGN_BOTTOM_RIGHT, -5, -5);
    lv_obj_set_style_bg_color(rej, lv_color_hex(0xCC2222), 0);
    lv_obj_set_style_radius(rej, 24, 0);
    lv_obj_add_event_cb(rej, ble_prompt_reject_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *rej_lbl = lv_label_create(rej);
    lv_label_set_text(rej_lbl, "Reject");
    lv_obj_set_style_text_color(rej_lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(rej_lbl, &lv_font_montserrat_16, 0);
    lv_obj_center(rej_lbl);

    ble_prompt_ticks = 0;
}

static void resqplus_display_set(bool on)
{
    if (on == resqplus_display_on)
    {
        return;
    }

    if (on)
    {
        (void)bsp_display_backlight_on();
        resqplus_display_on = true;
    }
    else
    {
        (void)bsp_display_backlight_off();
        resqplus_display_on = false;
    }
}

static void resqplus_activity(void)
{
    /* Wake display on any interaction */
    resqplus_display_set(true);
}

static void resqplus_screen_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_PRESSED || code == LV_EVENT_CLICKED)
    {
        resqplus_activity();
    }
}

static void IRAM_ATTR resqplus_boot_btn_isr_handler(void *arg)
{
    (void)arg;

    static volatile uint32_t last_tick;
    const uint32_t now_tick = (uint32_t)xTaskGetTickCountFromISR();

    /* Simple debounce: ignore edges within ~200ms */
    if ((now_tick - last_tick) < (uint32_t)pdMS_TO_TICKS(200))
    {
        return;
    }
    last_tick = now_tick;

    BaseType_t higher_prio_woken = pdFALSE;
    if (resqplus_boot_btn_task_handle)
    {
        vTaskNotifyGiveFromISR(resqplus_boot_btn_task_handle, &higher_prio_woken);
    }
    if (higher_prio_woken)
    {
        portYIELD_FROM_ISR();
    }
}

static void resqplus_boot_btn_task(void *arg)
{
    (void)arg;

    while (1)
    {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        /* LVGL updates must happen outside ISR. */
        bsp_display_lock(pdMS_TO_TICKS(200));
        resqplus_activity();
        resetAlertCount();
        resqplus_press_count = 0;
        resqplus_update_count_label();
        bsp_display_unlock();
    }
}

static void resqplus_init_boot_button(void)
{
    const gpio_num_t BOOT_BUTTON_GPIO = GPIO_NUM_0;

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BOOT_BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));

    esp_err_t ret = gpio_install_isr_service(0);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE)
    {
        ESP_ERROR_CHECK(ret);
    }

    ESP_ERROR_CHECK(gpio_isr_handler_add(BOOT_BUTTON_GPIO, resqplus_boot_btn_isr_handler, NULL));
}

static void resqplus_update_count_label(void)
{
    if (!resqplus_count_label)
    {
        return;
    }

    lv_label_set_text_fmt(resqplus_count_label, "Count: %d", resqplus_press_count);
}

static void resqplus_inc_btn_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED)
    {
        return;
    }

    resqplus_activity();

    // Send BLE Alert
    if (ble_server_is_connected())
    {
        ble_server_send_alert("Alert: Button Pressed!");
        LV_LOG_USER("Sent BLE Alert");
    }
    else
    {
        LV_LOG_USER("BLE not connected, cannot send alert");
    }

    resqplus_press_count++;
    resqplus_update_count_label();
}

static void resqplus_reset_btn_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED)
    {
        return;
    }

    resqplus_activity();
    resqplus_press_count = 0;
    resqplus_update_count_label();
}

static void IRAM_ATTR resqplus_pwr_btn_isr_handler(void *arg)
{
    (void)arg;

    static volatile uint32_t last_tick;
    const uint32_t now_tick = (uint32_t)xTaskGetTickCountFromISR();

    /* Simple debounce: ignore edges within ~200ms */
    if ((now_tick - last_tick) < (uint32_t)pdMS_TO_TICKS(200))
    {
        return;
    }
    last_tick = now_tick;

    BaseType_t higher_prio_woken = pdFALSE;
    if (resqplus_pwr_btn_task_handle)
    {
        vTaskNotifyGiveFromISR(resqplus_pwr_btn_task_handle, &higher_prio_woken);
    }
    if (higher_prio_woken)
    {
        portYIELD_FROM_ISR();
    }
}

static void resqplus_pwr_btn_task(void *arg)
{
    (void)arg;

    while (1)
    {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        if (resqplus_display_on)
        {
            resqplus_display_set(false);
        }
        else
        {
            resqplus_display_set(true);
        }
    }
}

static void resqplus_init_pwr_button(void)
{
    /* Board diagram shows PWR button on GPIO10 */
    const gpio_num_t PWR_BUTTON_GPIO = GPIO_NUM_10;

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << PWR_BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));

    esp_err_t ret = gpio_install_isr_service(0);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE)
    {
        ESP_ERROR_CHECK(ret);
    }

    ESP_ERROR_CHECK(gpio_isr_handler_add(PWR_BUTTON_GPIO, resqplus_pwr_btn_isr_handler, NULL));
}

static void resqplus_create_test_screen(void)
{
    lv_obj_t *scr = lv_screen_active();
    lv_obj_clean(scr);

    lv_obj_add_event_cb(scr, resqplus_screen_event_cb, LV_EVENT_PRESSED, NULL);

    resqplus_press_count = 0;
    resqplus_count_label = NULL;

    lv_obj_t *header = lv_label_create(scr);
    lv_label_set_text(header, "ResQPlus");
    lv_obj_set_style_text_font(header, LV_FONT_DEFAULT, 0);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 14);

    resqplus_count_label = lv_label_create(scr);
    lv_obj_align(resqplus_count_label, LV_ALIGN_TOP_MID, 0, 46);
    resqplus_update_count_label();

    lv_obj_t *inc_btn = lv_button_create(scr);
    lv_obj_set_size(inc_btn, 160, 56);
    lv_obj_align(inc_btn, LV_ALIGN_CENTER, 0, -10);
    lv_obj_add_event_cb(inc_btn, resqplus_inc_btn_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *inc_btn_label = lv_label_create(inc_btn);
    lv_label_set_text(inc_btn_label, "Send Alert");
    lv_obj_center(inc_btn_label);

    lv_obj_t *reset_btn = lv_button_create(scr);
    lv_obj_set_size(reset_btn, 160, 56);
    lv_obj_align(reset_btn, LV_ALIGN_CENTER, 0, 58);
    lv_obj_set_style_bg_color(reset_btn, lv_color_hex(0xFF0000), LV_PART_MAIN);
    lv_obj_set_style_text_color(reset_btn, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_add_event_cb(reset_btn, resqplus_reset_btn_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *reset_btn_label = lv_label_create(reset_btn);
    lv_label_set_text(reset_btn_label, "Reset");
    lv_obj_center(reset_btn_label);
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    lv_display_t *disp = bsp_display_start();
    if (disp)
    {
        bsp_display_backlight_on();
        display_width = lv_disp_get_hor_res(disp);
        display_height = lv_disp_get_ver_res(disp);
    }

    resqplus_display_on = true;

    bsp_display_lock(pdMS_TO_TICKS(200));
    ui_init();
    bsp_display_unlock();

    battery_monitor_start();

    /* Initialize BLE AFTER display so LCD framebuffers get internal DMA RAM first */
    ble_server_init();

    /* Poll for pending BLE connections every 500 ms and show approval prompt */
    bsp_display_lock(pdMS_TO_TICKS(200));
    lv_timer_create(ble_prompt_timer_cb, 500, NULL);
    /* Update clock labels every 1 second */
    lv_timer_create(clock_update_timer_cb, 1000, NULL);
    bsp_display_unlock();

    xTaskCreatePinnedToCore(
        resqplus_boot_btn_task,
        "resqplus_boot_btn",
        4096,
        NULL,
        5,
        &resqplus_boot_btn_task_handle,
        1);

    resqplus_init_boot_button();

    xTaskCreatePinnedToCore(
        resqplus_pwr_btn_task,
        "resqplus_pwr_btn",
        4096,
        NULL,
        5,
        &resqplus_pwr_btn_task_handle,
        1);

    resqplus_init_pwr_button();
}