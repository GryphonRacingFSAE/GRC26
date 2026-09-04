#include <GuiTask.h>
#include <PinDefs.h>
#include <Arduino.h>
#include <Wire.h>

// LVGL Includes
#define LV_CONF_INCLUDE_SIMPLE
#include <lv_conf.h>
#include <lvgl.h>
#include <esp_display_panel.hpp>
#include <esp_heap_caps.h>
#include <drivers/lcd/esp_panel_lcd_st7262.hpp>

#include "UI/ui_rpm.h"
#include "UI/ui_clt.h"
#include "UI/ui_battery_voltage.h"
#include "UI/ui_tps.h"
#include "UI/ui_bp.h"
#include "UI/ui_oil.h"

#define GUI_TASK_PERIOD_MS 30 // 33Hz Refresh

using namespace esp_panel::drivers;

LV_IMG_DECLARE(banner);

// Private Hardware Handles
static BusRGB* panel_bus = nullptr;
static LCD_ST7262* panel_lcd = nullptr;
static lv_disp_draw_buf_t draw_buf;
static lv_color_t* buf1 = nullptr;
static lv_color_t* buf2 = nullptr;

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
    QueueHandle_t data_queue = *params->dataQueue;

    Serial.println("[GUI] Init Started");

    // 1. Hardware Init
    BusRGB::RefreshPanelPartialConfig refresh_config = {
        .pclk_hz = 8 * 1000 * 1000,
        .h_res = 1024, .v_res = 600,
        .hsync_pulse_width = 20, .hsync_back_porch = 160, .hsync_front_porch = 200,
        .vsync_pulse_width = 3,  .vsync_back_porch = 12,  .vsync_front_porch = 12,
        .data_width = 16, .bits_per_pixel = 16, .bounce_buffer_size_px = 1024 * 10,
        .hsync_gpio_num = LCD_HSYNC, .vsync_gpio_num = LCD_VSYNC,
        .de_gpio_num = LCD_DE, .pclk_gpio_num = LCD_PCLK,
        .disp_gpio_num = -1,
        // Initialize as BRG so the lv_conf swap to RGB
        .data_gpio_nums = {
            // Blue
            LCD_B3, LCD_B4, LCD_B5, LCD_B6, LCD_B7, 
            // Green
            LCD_G2, LCD_G3, LCD_G4, LCD_G5, LCD_G6, LCD_G7,
            // Red
            LCD_R3, LCD_R4, LCD_R5, LCD_R6, LCD_R7
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
    // Allocate two buffer lines explicitly in SPIRAM for FPS performance
    uint16_t buf_size = 1024 * 50;
    buf1 = (lv_color_t*)heap_caps_malloc(buf_size * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    buf2 = (lv_color_t*)heap_caps_malloc(buf_size * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, buf_size);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = 1024;
    disp_drv.ver_res = 600;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    // 3. Create UI
    if (xSemaphoreTake(gui_mutex, portMAX_DELAY)) {
        // Screen init
        lv_obj_set_style_bg_color(lv_scr_act(), lv_color_black(), 0);
        // Format init
        ui_rpm_init();
        ui_clt_init();
        ui_battery_voltage_init();
        ui_tps_init();
        ui_bp_init();
        ui_oil_init();
        // Logo init 
        lv_obj_t* grc_logo = lv_img_create(lv_scr_act());
        lv_img_set_src(grc_logo, &banner);
        lv_obj_align(grc_logo, LV_ALIGN_BOTTOM_MID, 0, -50);
        lv_obj_set_size(grc_logo, 300, 98);
        xSemaphoreGive(gui_mutex);
    }
    
    Serial.println("[GUI] Loop Started");

    EcuData_t dataGui = {0};
    EcuData_t dataGui_prev = {0};
    // 4. Precise Loop
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(GUI_TASK_PERIOD_MS);

    for (;;) {
        vTaskDelayUntil(&xLastWakeTime, xFrequency);

        if (xSemaphoreTake(gui_mutex, 0) == pdTRUE) {
            bool update_status = false;

            while (xQueueReceive(data_queue, &dataGui, 0) == pdPASS) {
                update_status = true;
            }

            if (update_status) {
                if(dataGui.rpm != dataGui_prev.rpm) {
                    ui_rpm_update(&dataGui);
                }
                if(dataGui.clt != dataGui_prev.clt) {
                    ui_clt_update(&dataGui);
                }
                if(dataGui.batteryVoltage != dataGui_prev.batteryVoltage) {
                    ui_battery_voltage_update(&dataGui);
                }
                if(dataGui.tps != dataGui_prev.tps) {
                    ui_tps_update(&dataGui);
                }
                if(dataGui.bp != dataGui_prev.bp) {
                    ui_bp_update(&dataGui);
                }
                if(dataGui.oilPressure != dataGui_prev.oilPressure ||
                   dataGui.oilTemperature != dataGui_prev.oilTemperature) {
                    ui_oil_update(&dataGui);
                }

                dataGui_prev = dataGui;
            }

            lv_timer_handler();
            xSemaphoreGive(gui_mutex);
        }
    }
}
