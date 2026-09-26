#include "UI/ui_gear.h"

#define LV_CONF_INCLUDE_SIMPLE
#include <lv_conf.h>
#include <lvgl.h>

LV_FONT_DECLARE(lv_font_montserrat_140);

static constexpr int8_t marginY = -50;

static lv_obj_t* gear_label = nullptr;

void ui_gear_init() {
    gear_label = lv_label_create(lv_scr_act());

    lv_label_set_text(gear_label, "0");
    lv_obj_set_style_text_font(gear_label, &lv_font_montserrat_140, 0);
    lv_obj_set_style_text_color(gear_label, lv_color_white(), 0);
    lv_obj_align(gear_label, LV_ALIGN_CENTER, 0, marginY);  
}

void ui_gear_update(const EcuData_t* data) {
    static float gearPos_prev = 0;
    if(data->gearPos == gearPos_prev) {
        return; 
    }

    char gear_buf[8];
    snprintf(gear_buf, sizeof(gear_buf), "%d", data->gearPos);
    lv_label_set_text(gear_label, gear_buf);

    gearPos_prev = data->gearPos;
}
