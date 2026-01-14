#include <Arduino.h>
// LVGL INCLUDES
#define LV_CONF_INCLUDE_SIMPLE
#include <lv_conf.h>
#include <lvgl.h>

#include <esp_display_panel.hpp>
#include <drivers/lcd/esp_panel_lcd_st7262.hpp>

// LCD pins
static constexpr int8_t LCD_DE    =  5;
static constexpr int8_t LCD_VSYNC =  3;
static constexpr int8_t LCD_HSYNC =  46;
static constexpr int8_t LCD_PCLK  =  7;
// RGB Data pins
static constexpr int8_t LCD_R3    =  1;
static constexpr int8_t LCD_R4    =  2;
static constexpr int8_t LCD_R5    =  42;
static constexpr int8_t LCD_R6    =  41;
static constexpr int8_t LCD_R7    =  40;
static constexpr int8_t LCD_G2    =  39;
static constexpr int8_t LCD_G3    =  0;
static constexpr int8_t LCD_G4    =  45;
static constexpr int8_t LCD_G5    =  48;
static constexpr int8_t LCD_G6    =  47;
static constexpr int8_t LCD_G7    =  21;
static constexpr int8_t LCD_B3    =  14;
static constexpr int8_t LCD_B4    =  38;
static constexpr int8_t LCD_B5    =  18;
static constexpr int8_t LCD_B6    =  17;
static constexpr int8_t LCD_B7    =  10;


static constexpr int16_t LCD_WIDTH  = 1024;
static constexpr int16_t LCD_HEIGHT = 600;
// 8 Mhz pixel clock
static constexpr int32_t LCD_PCLK_HZ = 8 * 1000 * 1000;

using namespace esp_panel::drivers;

// GLOBAL OBJECTS 
BusRGB *panel_bus = nullptr;
LCD_ST7262 *panel_lcd = nullptr;

// LVGL BUFFERS (In PSRAM) 
#define DRAW_BUF_SIZE (LCD_WIDTH * 20)
static lv_disp_draw_buf_t draw_buf;

static lv_color_t *buf1 = nullptr;
static lv_color_t *buf2 = nullptr;

// LVGL FLUSH CALLBACK 
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
    int w = (area->x2 - area->x1 + 1);
    int h = (area->y2 - area->y1 + 1);

    panel_lcd->drawBitmap(area->x1, area->y1, w, h, (uint8_t *)color_p);

    lv_disp_flush_ready(disp);
}

void setup() {
    Serial.begin(115200);

    // Bus config
    BusRGB::RefreshPanelPartialConfig refresh_config = {
        .pclk_hz = LCD_PCLK_HZ,
        .h_res = LCD_WIDTH,
        .v_res = LCD_HEIGHT,
        .hsync_pulse_width = 20,
        .hsync_back_porch  = 160, 
        .hsync_front_porch = 200, 
        .vsync_pulse_width = 3,
        .vsync_back_porch  = 12,  
        .vsync_front_porch = 12,
        .data_width     = 16,
        .bits_per_pixel = 16,
        .bounce_buffer_size_px = 0, 
        .hsync_gpio_num = LCD_HSYNC,
        .vsync_gpio_num = LCD_VSYNC,
        .de_gpio_num    = LCD_DE,
        .pclk_gpio_num  = LCD_PCLK,
        .disp_gpio_num  = -1,
        .data_gpio_nums = {
            LCD_R3, LCD_R4, LCD_R5, LCD_R6, LCD_R7,
            LCD_G2, LCD_G3, LCD_G4, LCD_G5, LCD_G6, LCD_G7,
            LCD_B3, LCD_B4, LCD_B5, LCD_B6, LCD_B7
        },
    };

    BusRGB::Config bus_config;
    bus_config.refresh_panel = refresh_config;
    panel_bus = new BusRGB(bus_config);

    // LCD Config
    panel_lcd = new LCD_ST7262(panel_bus, LCD_WIDTH, LCD_HEIGHT, 16, -1);
    if (!panel_lcd->init()) { Serial.println("LCD Init Failed"); while(1); }
    panel_lcd->reset();
    panel_lcd->begin();

    lv_init();

    // Draw Buffer Allocation
    buf1 = (lv_color_t *)heap_caps_malloc(DRAW_BUF_SIZE * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    buf2 = (lv_color_t *)heap_caps_malloc(DRAW_BUF_SIZE * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, DRAW_BUF_SIZE);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = LCD_WIDTH;
    disp_drv.ver_res = LCD_HEIGHT;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_black(), 0);


    // Button (test)
    lv_obj_t *btn = lv_btn_create(lv_scr_act());
    lv_obj_align(btn, LV_ALIGN_CENTER, 0, 0);
    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, "Hello World!");
    lv_obj_set_style_text_color(label, lv_color_black(), 0);

    static lv_style_t style;
    lv_style_init(&style);
    lv_style_set_radius(&style, 10);          
    lv_style_set_bg_opa(&style, LV_OPA_COVER);
    lv_style_set_bg_color(&style, lv_palette_main(LV_PALETTE_BLUE));
    lv_style_set_bg_grad_dir(&style, LV_GRAD_DIR_NONE); 
    lv_style_set_shadow_width(&style, 0);               
    lv_style_set_border_width(&style, 0);               

    lv_obj_add_style(btn, &style, 0);

    Serial.println("LVGL Running");
}

void loop() {
    lv_timer_handler(); 
    delay(1);           
}