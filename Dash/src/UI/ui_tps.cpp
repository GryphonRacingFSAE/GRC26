#include "UI/ui_tps.h"

#define LV_CONF_INCLUDE_SIMPLE
#include <lv_conf.h>
#include <lvgl.h>

LV_FONT_DECLARE(lv_font_montserrat_96);

static constexpr int8_t marginX = 50;
static constexpr int8_t marginY = 50;

static lv_obj_t* throttle_label = nullptr;
static lv_obj_t* throttle_ui = nullptr;

void ui_tps_init () {
    throttle_label = lv_label_create(lv_scr_act());
    throttle_ui = lv_label_create(lv_scr_act());

    // if (!throttle_label || !throttle_ui) { 
    //     Serial.println("[GUI] throttle_label alloc failed"); 
    //     return;
    // }

    lv_label_set_text(throttle_label, "0%");
    lv_obj_set_style_text_font(throttle_label, &lv_font_montserrat_96, 0);
    lv_obj_set_style_text_color(throttle_label, lv_color_white(), 0);
    lv_obj_align(throttle_label, LV_ALIGN_BOTTOM_LEFT, marginX, -2*marginY);  

    lv_obj_update_layout(throttle_label);

    lv_label_set_text(throttle_ui, "TPS");
    lv_obj_set_style_text_font(throttle_ui, &lv_font_montserrat_36, 0);
    lv_obj_set_style_text_color(throttle_ui, lv_color_white(), 0);
    lv_obj_align_to(throttle_ui, throttle_label, LV_ALIGN_OUT_BOTTOM_MID, 0, marginY/2);
}

void ui_tps_update(const EcuData_t* data) {
    static uint16_t tps_prev = 0;
    if(data->tps == tps_prev) {
        return; 
    }

    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", data->tps);
    lv_label_set_text(throttle_label, buf);
    tps_prev = data->tps;
}
