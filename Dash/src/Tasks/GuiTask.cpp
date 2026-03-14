#include <GuiTask.h>
#include <PinDefs.h>
#include <Arduino.h>
#include <Wire.h>
#include <iostream>
#include <algorithm>

// LVGL Includes
#define LV_CONF_INCLUDE_SIMPLE
#include <lv_conf.h>
#include <lvgl.h>
#include <esp_display_panel.hpp>
#include <drivers/lcd/esp_panel_lcd_st7262.hpp>

#define GUI_TASK_PERIOD_MS 5 // 200Hz Refresh

using namespace esp_panel::drivers;

// Global UI Labels
static lv_obj_t* rpm_label = nullptr;
static lv_obj_t* speed_label = nullptr;
static lv_obj_t* throttle_label = nullptr;
static lv_obj_t* coolantTemp_label = nullptr;
static lv_obj_t* oilTemp_label = nullptr;

// Global UI State
static uint16_t rpm_value = 0;
static uint8_t speed_value = 0;  
static uint8_t throttle_value = 0;
static float coolantTemp_value = 0;
static float oilTemp_value = 0;

/**
 * Car Numerical Value Callback Ui:
 * Timer callback to update the counter value and label text every 100ms
 * Testing LVGL timers and dynamic label updates. Will be used for periodic UI updates in the future.
 * Ignore stuff like "%8d", it's just for formatting the text to look nice on the LCD.
 */

static void rpm_cb(lv_timer_t* timer) {
    rpm_value += 1000;  
    lv_label_set_text_fmt(rpm_label, "%d\n", rpm_value);
}

static void carspeed_cb(lv_timer_t* timer) {
    speed_value++;  
    lv_label_set_text_fmt(speed_label, "%d\n", speed_value);
} 

static void throttle_cb(lv_timer_t* timer) {
    throttle_value++;  
    lv_label_set_text_fmt(throttle_label, "Throttle pos: %8d%%", throttle_value);
}

static void coolantTemp_cb(lv_timer_t* timer) {
    coolantTemp_value = 108.69;
    char buf[32];
    snprintf(buf, sizeof(buf), "Coolant Temp: \t%.2f°C", coolantTemp_value);
    lv_label_set_text(coolantTemp_label, buf);

    if(coolantTemp_value > 107.0f) {
        lv_obj_set_style_text_color(coolantTemp_label, lv_color_hex(0xFFCE1B), 0);
    } else if(coolantTemp_value > 115.0f) {
        lv_obj_set_style_text_color(coolantTemp_label, lv_color_hex(0xFF2C2C), 0);
    } else {
        lv_obj_set_style_text_color(coolantTemp_label, lv_color_white(), 0);
    }
}

static void oilTemp_cb(lv_timer_t* timer) {
    oilTemp_value = 167.67;  
    char buf[32];
    snprintf(buf, sizeof(buf), "Oil Temp: %18.2f°C", oilTemp_value);
    lv_label_set_text(oilTemp_label, buf);

    if(oilTemp_value > 110.0f) {
        lv_obj_set_style_text_color(oilTemp_label, lv_color_hex(0xFFCE1B), 0);
    } else if(oilTemp_value > 121.0f) {
        lv_obj_set_style_text_color(oilTemp_label, lv_color_hex(0xFF2C2C), 0);
    } else {
        lv_obj_set_style_text_color(oilTemp_label, lv_color_white(), 0);
    }
}

/**
 * Car Label Format Callback Ui:
 * Formatting the value displayed on LCD 
 */

static void rpm_format () {
    rpm_label = lv_label_create(lv_scr_act());
    lv_label_set_text(rpm_label, "0");
    lv_obj_align(rpm_label, LV_ALIGN_CENTER, 0, -250);  
    lv_obj_set_style_text_font(rpm_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(rpm_label, lv_color_white(), 0);
}

static void carspeed_format () {
    speed_label = lv_label_create(lv_scr_act());
    lv_label_set_text(speed_label, "0");
    lv_obj_align(speed_label, LV_ALIGN_CENTER, 0, -150);  
    lv_obj_set_style_text_font(speed_label, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(speed_label, lv_color_white(), 0);
}

static void throttle_format () {
    throttle_label = lv_label_create(lv_scr_act());
    lv_label_set_text(throttle_label, "0%");
    lv_obj_align(throttle_label, LV_ALIGN_LEFT_MID, 50, 0);  
    lv_obj_set_style_text_font(throttle_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(throttle_label, lv_color_white(), 0);
}

static void coolantTemp_format () {
    coolantTemp_label = lv_label_create(lv_scr_act());
    lv_label_set_text(coolantTemp_label, "0°C");
    lv_obj_align(coolantTemp_label, LV_ALIGN_LEFT_MID, 50, 50);  
    lv_obj_set_style_text_font(coolantTemp_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(coolantTemp_label, lv_color_white(), 0);
}

static void oilTemp_format () {
    oilTemp_label = lv_label_create(lv_scr_act());
    lv_label_set_text(oilTemp_label, "0°C");
    lv_obj_align(oilTemp_label, LV_ALIGN_LEFT_MID, 50, 100);  
    lv_obj_set_style_text_font(oilTemp_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(oilTemp_label, lv_color_white(), 0);
}

// Private Hardware Handles
static BusRGB* panel_bus = nullptr;
static LCD_ST7262* panel_lcd = nullptr;
static lv_disp_draw_buf_t draw_buf;
static lv_color_t* buf1 = nullptr;

// Flush Callback
static void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
    int w = (area->x2 - area->x1 + 1);
    int h = (area->y2 - area->y1 + 1);
    panel_lcd->drawBitmap(area->x1, area->y1, w, h, (uint8_t *)color_p);
    lv_disp_flush_ready(disp);
}

void GuiTask(void* pvParameters) {
    GuiTaskParameters* params = (GuiTaskParameters*)pvParameters;
    SemaphoreHandle_t gui_mutex = *params->guiMutex;

    Serial.println("[GUI] Init Started");

    // 1. Hardware Init
    BusRGB::RefreshPanelPartialConfig refresh_config = {
        .pclk_hz = 8 * 1000 * 1000,
        .h_res = 1024, .v_res = 600,
        .hsync_pulse_width = 20, .hsync_back_porch = 160, .hsync_front_porch = 200,
        .vsync_pulse_width = 3,  .vsync_back_porch = 12,  .vsync_front_porch = 12,
        .data_width = 16, .bits_per_pixel = 16, .bounce_buffer_size_px = 0,
        .hsync_gpio_num = LCD_HSYNC, .vsync_gpio_num = LCD_VSYNC,
        .de_gpio_num = LCD_DE, .pclk_gpio_num = LCD_PCLK,
        .disp_gpio_num = -1,
        .data_gpio_nums = {
            LCD_R3, LCD_R4, LCD_R5, LCD_R6, LCD_R7,
            LCD_G2, LCD_G3, LCD_G4, LCD_G5, LCD_G6, LCD_G7,
            LCD_B3, LCD_B4, LCD_B5, LCD_B6, LCD_B7
        },
    };
    BusRGB::Config bus_config;
    bus_config.refresh_panel = refresh_config;
    panel_bus = new BusRGB(bus_config);
    panel_lcd = new LCD_ST7262(panel_bus, 1024, 600, 16, -1);
    panel_lcd->init();
    panel_lcd->reset();
    panel_lcd->begin();

    // 2. LVGL Init
    lv_init();
    // Internal RAM buffer (1024 * 40 pixels)
    buf1 = (lv_color_t*)malloc(1024 * 40 * sizeof(lv_color_t));
    lv_disp_draw_buf_init(&draw_buf, buf1, NULL, 1024 * 40);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = 1024;
    disp_drv.ver_res = 600;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    // 3. Create UI
    if (xSemaphoreTake(gui_mutex, portMAX_DELAY)) {
        // // Screen init
        lv_obj_set_style_bg_color(lv_scr_act(), lv_color_black(), 0);
        carspeed_format();
        rpm_format();
        throttle_format();
        coolantTemp_format();
        oilTemp_format();

        lv_timer_create(rpm_cb, 500, NULL);
        lv_timer_create(carspeed_cb, 1000, NULL);
        lv_timer_create(throttle_cb, 500, NULL);
        lv_timer_create(coolantTemp_cb, 2000, NULL);
        lv_timer_create(oilTemp_cb, 2000, NULL);

        xSemaphoreGive(gui_mutex);
    }
    
    Serial.println("[GUI] Loop Started");

    // 4. Precise Loop
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(GUI_TASK_PERIOD_MS);

    for (;;) {
        vTaskDelayUntil(&xLastWakeTime, xFrequency);

        if (xSemaphoreTake(gui_mutex, 0) == pdTRUE) {
            Serial.println("GUI Task");
            lv_timer_handler();
            xSemaphoreGive(gui_mutex);
        }
    }
}