#include "UI/ui_clt.h"

#define LV_CONF_INCLUDE_SIMPLE
#include <lv_conf.h>
#include <lvgl.h>

LV_FONT_DECLARE(lv_font_montserrat_96);

static constexpr int8_t marginX = 50;
static constexpr int8_t marginY = 50;

static lv_obj_t* coolantTemp_label = nullptr;
static lv_obj_t* coolantTemp_ui = nullptr; 

static EcuData_t clt_gui = {0};

static void coolantTemp_cb(lv_timer_t* timer) {
    char buf[8];
    snprintf(buf, sizeof(buf), "%d°C", clt_gui.clt);
    lv_label_set_text(coolantTemp_label, buf);

    if(clt_gui.clt > 115) {
        lv_obj_set_style_text_color(coolantTemp_label, lv_color_hex(0xFF2C2C), 0);
    } else if(clt_gui.clt > 107) {
        lv_obj_set_style_text_color(coolantTemp_label, lv_color_hex(0xFFCE1B), 0);
    } else {
        lv_obj_set_style_text_color(coolantTemp_label, lv_color_white(), 0);
    }
}

void ui_clt_init() {
    coolantTemp_label = lv_label_create(lv_scr_act());
    coolantTemp_ui = lv_label_create(lv_scr_act());

    // if (!coolantTemp_label || !coolantTemp_ui) { 
    //     Serial.println("[GUI] coolantTemp_label alloc failed"); 
    //     return;
    // }

    lv_label_set_text(coolantTemp_label, "0°C");
    lv_obj_set_style_text_font(coolantTemp_label, &lv_font_montserrat_96, 0);
    lv_obj_set_style_text_color(coolantTemp_label, lv_color_white(), 0);
    lv_obj_align(coolantTemp_label, LV_ALIGN_TOP_LEFT, marginX, marginY);  

    lv_obj_update_layout(coolantTemp_label);

    lv_label_set_text(coolantTemp_ui, "CLT");
    lv_obj_set_style_text_font(coolantTemp_ui, &lv_font_montserrat_36, 0);
    lv_obj_set_style_text_color(coolantTemp_ui, lv_color_white(), 0);
    lv_obj_align_to(coolantTemp_ui, coolantTemp_label, LV_ALIGN_OUT_BOTTOM_MID, 0, marginY/2);

    lv_timer_create(coolantTemp_cb, 1021, NULL);
}

void ui_clt_update(const EcuData_t* data) {
    clt_gui = *data;
}
