#include "UI/ui_apps.h"

#define LV_CONF_INCLUDE_SIMPLE
#include <lv_conf.h>
#include <lvgl.h>

static constexpr int16_t METER_W = 100;
static constexpr int16_t METER_H = 400;
static constexpr int16_t METER_RIGHT_X = 100;   
static constexpr int16_t METER_SPACING = 50;
 
static lv_obj_t* apps_bar   = nullptr;
static lv_obj_t* apps_label = nullptr;
static lv_obj_t* apps_ui    = nullptr; 

static inline void set_bar_color(lv_obj_t* bar, lv_color_t color) {
    lv_obj_set_style_bg_color(bar, color, LV_PART_INDICATOR);
}

static void apps_format() {
    apps_bar = lv_bar_create(lv_scr_act());
    lv_obj_set_size(apps_bar, METER_W, METER_H);
    lv_bar_set_range(apps_bar, 0, 100);
    lv_bar_set_value(apps_bar, 0, LV_ANIM_OFF);
 
    lv_obj_set_style_bg_color(apps_bar, lv_color_hex(0x1E1E1E), LV_PART_MAIN);
    lv_obj_set_style_border_color(apps_bar, lv_color_hex(0x4A4A4A), LV_PART_MAIN);
    lv_obj_set_style_border_width(apps_bar, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(apps_bar, 6, LV_PART_MAIN);
 
    lv_obj_set_style_bg_color(apps_bar, lv_color_hex(0x00C853), LV_PART_INDICATOR);
    lv_obj_set_style_radius(apps_bar, 4, LV_PART_INDICATOR);
 
    lv_obj_align(apps_bar, LV_ALIGN_RIGHT_MID,-(METER_SPACING), 0);
 
    apps_label = lv_label_create(lv_scr_act());
    lv_label_set_text(apps_label, "%");
    lv_obj_set_style_text_font(apps_label, &lv_font_montserrat_36, 0);
    lv_obj_set_style_text_color(apps_label, lv_color_white(), 0);
    lv_obj_align_to(apps_label, apps_bar, LV_ALIGN_OUT_TOP_MID, 0, -10);
 
    apps_ui = lv_label_create(lv_scr_act());
    lv_label_set_text(apps_ui, "APPS");
    lv_obj_set_style_text_font(apps_ui, &lv_font_montserrat_36, 0);
    lv_obj_set_style_text_color(apps_ui, lv_color_white(), 0);
    lv_obj_align_to(apps_ui, apps_bar, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);
}

void ui_apps_init() {
    apps_format();
}

void ui_apps_update(const EcuData_t* data) {
    static uint16_t apps_prev = 0;
    if(data->apps == apps_prev) {
        return; 
    }

    uint8_t val = (data->apps > 100) ? 100 : data->apps;
 
    lv_bar_set_value(apps_bar, val, LV_ANIM_OFF);
    lv_label_set_text_fmt(apps_label, "%d%%", val);
 
    lv_color_t col = (val < 50) ? lv_color_hex(0x00C853)   // green
                   : (val < 80) ? lv_color_hex(0xFFD600)   // amber
                   :              lv_color_hex(0xFF1744);   // red
    set_bar_color(apps_bar, col);

    apps_prev = data->apps;
}
