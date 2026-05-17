#include "UI/ui_bp.h"

#define LV_CONF_INCLUDE_SIMPLE
#include <lv_conf.h>
#include <lvgl.h>

// Meter UI
static constexpr int16_t METER_W = 100;
static constexpr int16_t METER_H = 400;
static constexpr int16_t METER_RIGHT_X = 100;   
static constexpr int16_t METER_SPACING = 50;
static constexpr float BRAKE_PRES_MAX = 2000.0f;
 
static lv_obj_t* brake_bar   = nullptr;
static lv_obj_t* brake_label = nullptr;
static lv_obj_t* brake_ui    = nullptr; 

static inline void set_bar_color(lv_obj_t* bar, lv_color_t color) {
    lv_obj_set_style_bg_color(bar, color, LV_PART_INDICATOR);
}

static void brake_format() {
    brake_bar = lv_bar_create(lv_scr_act());
    lv_obj_set_size(brake_bar, METER_W, METER_H);
    lv_bar_set_range(brake_bar, 0, 100);
    lv_bar_set_value(brake_bar, 0, LV_ANIM_OFF);
 
    lv_obj_set_style_bg_color(brake_bar, lv_color_hex(0x1E1E1E), LV_PART_MAIN);
    lv_obj_set_style_border_color(brake_bar, lv_color_hex(0x4A4A4A), LV_PART_MAIN);
    lv_obj_set_style_border_width(brake_bar, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(brake_bar, 6, LV_PART_MAIN);
 
    lv_obj_set_style_bg_color(brake_bar, lv_color_hex(0x2979FF), LV_PART_INDICATOR);
    lv_obj_set_style_radius(brake_bar, 4, LV_PART_INDICATOR);
 
    lv_obj_align(brake_bar, LV_ALIGN_RIGHT_MID,-(METER_RIGHT_X + METER_W), 0);
 
    brake_label = lv_label_create(lv_scr_act());
    lv_label_set_text(brake_label, "0");
    lv_obj_set_style_text_font(brake_label, &lv_font_montserrat_36, 0);
    lv_obj_set_style_text_color(brake_label, lv_color_white(), 0);
    lv_obj_align_to(brake_label, brake_bar, LV_ALIGN_OUT_TOP_MID, 0, -10);
 
    brake_ui = lv_label_create(lv_scr_act());
    lv_label_set_text(brake_ui, "Brake");
    lv_obj_set_style_text_font(brake_ui, &lv_font_montserrat_36, 0);
    lv_obj_set_style_text_color(brake_ui, lv_color_white(), 0);
    lv_obj_align_to(brake_ui, brake_bar, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);
}

void ui_bp_init() {
    brake_format();
}

void ui_bp_update(const EcuData_t* data) {
    static uint16_t bp_prev = 0;
    if(data->bp == bp_prev) {
        return; 
    }

    int32_t pct = (int32_t)((data->bp / BRAKE_PRES_MAX) * 100.0f);
    pct = (pct > 100) ? 100 : (pct < 0) ? 0 : pct;
 
    lv_bar_set_value(brake_bar, pct, LV_ANIM_OFF);
    lv_label_set_text_fmt(brake_label, "%d", pct);
 
    lv_color_t col = (pct < 30) ? lv_color_hex(0x2979FF) : (pct < 70) ? lv_color_hex(0xAA00FF) : lv_color_hex(0xFF1744);  
    set_bar_color(brake_bar, col);

    bp_prev = data->bp;
}
