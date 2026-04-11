#include "UI/ui_battery_voltage.h"

#define LV_CONF_INCLUDE_SIMPLE
#include <lv_conf.h>
#include <lvgl.h>
#include <math.h>

LV_FONT_DECLARE(lv_font_montserrat_96);

static constexpr int8_t marginX = 50;
static constexpr int8_t marginY = 50;

static lv_obj_t* battery_voltage_label = nullptr;
static lv_obj_t* battery_voltage_ui = nullptr;

void ui_battery_voltage_init () {
    battery_voltage_label = lv_label_create(lv_scr_act());
    battery_voltage_ui = lv_label_create(lv_scr_act());

    lv_label_set_text(battery_voltage_label, "0.0V");
    lv_obj_set_style_text_font(battery_voltage_label, &lv_font_montserrat_96, 0);
    lv_obj_set_style_text_color(battery_voltage_label, lv_color_white(), 0);
    lv_obj_align(battery_voltage_label, LV_ALIGN_BOTTOM_LEFT, marginX, -2*marginY);  

    lv_obj_update_layout(battery_voltage_label);

    lv_label_set_text(battery_voltage_ui, "Battery");
    lv_obj_set_style_text_font(battery_voltage_ui, &lv_font_montserrat_36, 0);
    lv_obj_set_style_text_color(battery_voltage_ui, lv_color_white(), 0);
    lv_obj_align_to(battery_voltage_ui, battery_voltage_label, LV_ALIGN_OUT_BOTTOM_MID, 0, marginY/2);
}

void ui_battery_voltage_update(const EcuData_t* data) {
    static float battery_voltage_prev = 0;
    if(data->battery_voltage == battery_voltage_prev) {
        return; 
    }

    char buf[8];
    snprintf(buf, sizeof(buf), "%.1fV", data->battery_voltage);
    lv_label_set_text(battery_voltage_label, buf);
    battery_voltage_prev = data->battery_voltage;
}