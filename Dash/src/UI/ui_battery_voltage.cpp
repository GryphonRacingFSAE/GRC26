#include "UI/ui_battery_voltage.h"

#define LV_CONF_INCLUDE_SIMPLE
#include <lv_conf.h>
#include <lvgl.h>

LV_FONT_DECLARE(lv_font_montserrat_96);

static constexpr int8_t marginX = 25;
static constexpr int8_t marginY = 50;
static constexpr int8_t CAPTION_GAP = 4;

static lv_obj_t* battery_voltage_label = nullptr;
static lv_obj_t* battery_voltage_ui = nullptr;

void ui_battery_voltage_init () {
    battery_voltage_label = lv_label_create(lv_scr_act());
    battery_voltage_ui = lv_label_create(lv_scr_act());

    lv_label_set_text(battery_voltage_label, "0.0");
    lv_obj_set_style_text_font(battery_voltage_label, &lv_font_montserrat_96, 0);
    lv_obj_set_style_text_color(battery_voltage_label, lv_color_white(), 0);
    lv_obj_align(battery_voltage_label, LV_ALIGN_BOTTOM_LEFT, marginX, -2*marginY);  

    lv_obj_update_layout(battery_voltage_label);

    lv_label_set_text(battery_voltage_ui, "BAT V");
    lv_obj_set_style_text_font(battery_voltage_ui, &lv_font_montserrat_36, 0);
    lv_obj_set_style_text_color(battery_voltage_ui, lv_color_white(), 0);
    lv_obj_align_to(battery_voltage_ui, battery_voltage_label, LV_ALIGN_OUT_BOTTOM_MID, 0, CAPTION_GAP);
}

void ui_battery_voltage_update(const EcuData_t* data) {
    static float batteryVoltage_prev = 0;
    if(data->batteryVoltage == batteryVoltage_prev) {
        return; 
    }

    char buf[8];
    snprintf(buf, sizeof(buf), "%.1fV", data->batteryVoltage);
    lv_label_set_text(battery_voltage_label, buf);
    batteryVoltage_prev = data->batteryVoltage;
}