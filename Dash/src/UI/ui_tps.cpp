#include "UI/ui_tps.h"

#define LV_CONF_INCLUDE_SIMPLE
#include <lv_conf.h>
#include <lvgl.h>

static constexpr int16_t METER_W = 100;
static constexpr int16_t METER_H = 400;
static constexpr int16_t METER_RIGHT_X = 100;   
static constexpr int16_t METER_SPACING = 50;
 
static lv_obj_t* tps_bar   = nullptr;
static lv_obj_t* tps_label = nullptr;
static lv_obj_t* tps_ui    = nullptr; 

static inline void set_bar_color(lv_obj_t* bar, lv_color_t color) {
    lv_obj_set_style_bg_color(bar, color, LV_PART_INDICATOR);
}

static void tps_format() {
    tps_bar = lv_bar_create(lv_scr_act());
    lv_obj_set_size(tps_bar, METER_W, METER_H);
    lv_bar_set_range(tps_bar, 0, 100);
    lv_bar_set_value(tps_bar, 0, LV_ANIM_OFF);
 
    lv_obj_set_style_bg_color(tps_bar, lv_color_hex(0x1E1E1E), LV_PART_MAIN);
    lv_obj_set_style_border_color(tps_bar, lv_color_hex(0x4A4A4A), LV_PART_MAIN);
    lv_obj_set_style_border_width(tps_bar, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(tps_bar, 6, LV_PART_MAIN);
 
    lv_obj_set_style_bg_color(tps_bar, lv_color_hex(0x00C853), LV_PART_INDICATOR);
    lv_obj_set_style_radius(tps_bar, 4, LV_PART_INDICATOR);
 
    lv_obj_align(tps_bar, LV_ALIGN_RIGHT_MID,-(METER_SPACING), 0);
 
    tps_label = lv_label_create(lv_scr_act());
    lv_label_set_text(tps_label, "%");
    lv_obj_set_style_text_font(tps_label, &lv_font_montserrat_36, 0);
    lv_obj_set_style_text_color(tps_label, lv_color_white(), 0);
    lv_obj_align_to(tps_label, tps_bar, LV_ALIGN_OUT_TOP_MID, 0, -10);
 
    tps_ui = lv_label_create(lv_scr_act());
    lv_label_set_text(tps_ui, "TPS");
    lv_obj_set_style_text_font(tps_ui, &lv_font_montserrat_36, 0);
    lv_obj_set_style_text_color(tps_ui, lv_color_white(), 0);
    lv_obj_align_to(tps_ui, tps_bar, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);
}

void ui_tps_init() {
    tps_format();
}

void ui_tps_update(const EcuData_t* data) {
    static uint16_t tps_prev = 0;
    if(data->tps == tps_prev) {
        return; 
    }

    uint8_t val = (data->tps > 100) ? 100 : data->tps;
 
    lv_bar_set_value(tps_bar, val, LV_ANIM_OFF);
    lv_label_set_text_fmt(tps_label, "%d%%", val);
 
    lv_color_t col = (val < 50) ? lv_color_hex(0x00C853)   // green
                   : (val < 80) ? lv_color_hex(0xFFD600)   // amber
                   :              lv_color_hex(0xFF1744);   // red
    set_bar_color(tps_bar, col);

    tps_prev = data->tps;
}
