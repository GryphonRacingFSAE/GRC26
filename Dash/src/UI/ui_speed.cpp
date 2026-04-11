#include "UI/ui_speed.h"

#define LV_CONF_INCLUDE_SIMPLE
#include <lv_conf.h>
#include <lvgl.h>
#include <math.h>

LV_FONT_DECLARE(lv_font_montserrat_140);

static constexpr int8_t marginY = 25;

static lv_obj_t* speed_label = nullptr;
static lv_obj_t* speed_ui = nullptr;

void ui_speed_init() {
    speed_label = lv_label_create(lv_scr_act());
    speed_ui = lv_label_create(lv_scr_act());

    lv_label_set_text(speed_label, "0");
    lv_obj_set_style_text_font(speed_label, &lv_font_montserrat_140, 0);
    lv_obj_set_style_text_color(speed_label, lv_color_white(), 0);
    lv_obj_align(speed_label, LV_ALIGN_CENTER, 0, 0);  

    lv_obj_update_layout(speed_label);

    lv_label_set_text(speed_ui, "km/h");
    lv_obj_set_style_text_font(speed_ui, &lv_font_montserrat_36, 0);
    lv_obj_set_style_text_color(speed_ui, lv_color_white(), 0);
    lv_obj_align_to(speed_ui, speed_label, LV_ALIGN_OUT_BOTTOM_MID, 0, marginY);
}

void ui_speed_update(const EcuData_t* data) {
    static float speed_prev = 0;
    if(data->speed == speed_prev) {
        return; 
    }

    char speed_buf[8];
    snprintf(speed_buf, sizeof(speed_buf), "%d", (int)round(data->speed));
    lv_label_set_text(speed_label, speed_buf);

    speed_prev = data->speed;
}
