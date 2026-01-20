#ifndef PIN_DEFS_H
#define PIN_DEFS_H

#include <stdint.h>

// LCD pins
static constexpr int8_t LCD_DE    =  5;
static constexpr int8_t LCD_VSYNC =  3;
static constexpr int8_t LCD_HSYNC =  46;
static constexpr int8_t LCD_PCLK  =  7;
// RGB Data pins
static constexpr int8_t LCD_R3    =  1;
static constexpr int8_t LCD_R4    =  2;
static constexpr int8_t LCD_R5    =  42;
static constexpr int8_t LCD_R6    =  41;
static constexpr int8_t LCD_R7    =  40;
static constexpr int8_t LCD_G2    =  39;
static constexpr int8_t LCD_G3    =  0;
static constexpr int8_t LCD_G4    =  45;
static constexpr int8_t LCD_G5    =  48;
static constexpr int8_t LCD_G6    =  47;
static constexpr int8_t LCD_G7    =  21;
static constexpr int8_t LCD_B3    =  14;
static constexpr int8_t LCD_B4    =  38;
static constexpr int8_t LCD_B5    =  18;
static constexpr int8_t LCD_B6    =  17;
static constexpr int8_t LCD_B7    =  10;
// LED PINS
// NOTE: These pins are connected to RS485 breakout on the board but will be used for the LED
static constexpr int8_t LED_DATA_PIN = 43;

#endif // PIN_DEFS_H