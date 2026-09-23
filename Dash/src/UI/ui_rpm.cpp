#include "UI/ui_rpm.h"

#define LV_CONF_INCLUDE_SIMPLE
#include <lv_conf.h>
#include <lvgl.h>

LV_FONT_DECLARE(lv_font_montserrat_140);

static constexpr int8_t marginY = 25;

static lv_obj_t* rpm_label = nullptr;
static lv_obj_t* rpm_ui = nullptr;

void ui_rpm_init() {
    rpm_label = lv_label_create(lv_scr_act());
    rpm_ui = lv_label_create(lv_scr_act());

    lv_label_set_text(rpm_label, "0");
    lv_obj_set_style_text_font(rpm_label, &lv_font_montserrat_140, 0);
    lv_obj_set_style_text_color(rpm_label, lv_color_white(), 0);
    lv_obj_align(rpm_label, LV_ALIGN_CENTER, 0, -2*marginY);  

    lv_obj_update_layout(rpm_label);

    lv_label_set_text(rpm_ui, "RPM");
    lv_obj_set_style_text_font(rpm_ui, &lv_font_montserrat_36, 0);
    lv_obj_set_style_text_color(rpm_ui, lv_color_white(), 0);
    lv_obj_align_to(rpm_ui, rpm_label, LV_ALIGN_OUT_BOTTOM_MID, 0, marginY);
}

void ui_rpm_update(const EcuData_t* data) {
    static float rpm_prev = 0;
    if(data->rpm == rpm_prev) {
        return; 
    }

    char rpm_buf[8];
    snprintf(rpm_buf, sizeof(rpm_buf), "%d", data->rpm);
    lv_label_set_text(rpm_label, rpm_buf);

    rpm_prev = data->rpm;
}
