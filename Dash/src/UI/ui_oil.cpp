#include "UI/ui_oil.h"

#define LV_CONF_INCLUDE_SIMPLE
#include <lv_conf.h>
#include <lvgl.h>

static constexpr int16_t OIL_X = 50;
static constexpr int16_t OIL_PRESSURE_Y = -45;
static constexpr int16_t OIL_TEMPERATURE_Y = 30;
static constexpr int16_t CAPTION_GAP = 4;

static lv_obj_t* oil_pressure_label = nullptr;
static lv_obj_t* oil_pressure_ui = nullptr;
static lv_obj_t* oil_temperature_label = nullptr;
static lv_obj_t* oil_temperature_ui = nullptr;

static void set_label_style(lv_obj_t* label, const lv_font_t* font) {
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
}

void ui_oil_init() {
    oil_pressure_label = lv_label_create(lv_scr_act());
    oil_pressure_ui = lv_label_create(lv_scr_act());
    oil_temperature_label = lv_label_create(lv_scr_act());
    oil_temperature_ui = lv_label_create(lv_scr_act());

    lv_label_set_text(oil_pressure_label, "0.0");
    set_label_style(oil_pressure_label, &lv_font_montserrat_36);
    lv_obj_align(oil_pressure_label, LV_ALIGN_LEFT_MID, OIL_X, OIL_PRESSURE_Y);

    lv_label_set_text(oil_pressure_ui, "Oil kPa");
    set_label_style(oil_pressure_ui, &lv_font_montserrat_18);
    lv_obj_align_to(oil_pressure_ui, oil_pressure_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, CAPTION_GAP);

    lv_label_set_text(oil_temperature_label, "0.0");
    set_label_style(oil_temperature_label, &lv_font_montserrat_36);
    lv_obj_align(oil_temperature_label, LV_ALIGN_LEFT_MID, OIL_X, OIL_TEMPERATURE_Y);

    lv_label_set_text(oil_temperature_ui, "Oil \xC2\xB0" "C");
    set_label_style(oil_temperature_ui, &lv_font_montserrat_18);
    lv_obj_align_to(oil_temperature_ui, oil_temperature_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, CAPTION_GAP);
}

void ui_oil_update(const EcuData_t* data) {
    static float oilPressure_prev = 0;
    static float oilTemperature_prev = 0;

    if(data->oilPressure != oilPressure_prev) {
        char pressure_buf[8];
        snprintf(pressure_buf, sizeof(pressure_buf), "%.1f", data->oilPressure);
        lv_label_set_text(oil_pressure_label, pressure_buf);
        oilPressure_prev = data->oilPressure;
    }

    if(data->oilTemperature != oilTemperature_prev) {
        char temperature_buf[9];
        snprintf(temperature_buf, sizeof(temperature_buf), "%.1f", data->oilTemperature);
        lv_label_set_text(oil_temperature_label, temperature_buf);
        oilTemperature_prev = data->oilTemperature;
    }
}
