#ifndef TEST_TWAI_H
#define TEST_TWAI_H

#include <stdint.h>

using gpio_num_t = int;
using esp_err_t = int;
constexpr esp_err_t ESP_OK = 0;
constexpr esp_err_t ESP_ERR_INVALID_STATE = 1;
constexpr esp_err_t ESP_FAIL = 2;

enum twai_mode_t { TWAI_MODE_NORMAL, TWAI_MODE_NO_ACK, TWAI_MODE_LISTEN_ONLY };

struct twai_general_config_t {
    gpio_num_t tx_io;
    gpio_num_t rx_io;
    twai_mode_t mode;
};

struct twai_timing_config_t {
    uint32_t bitrate;
};

struct twai_filter_config_t {
    bool accept_all;
};

struct twai_message_t {
    uint32_t identifier;
    bool extd;
    bool rtr;
    uint8_t data_length_code;
    uint8_t data[8];
};

#define TWAI_GENERAL_CONFIG_DEFAULT(tx, rx, mode)                                                                      \
    twai_general_config_t                                                                                              \
    {                                                                                                                  \
        tx, rx, mode                                                                                                   \
    }
#define TWAI_TIMING_CONFIG_500KBITS() twai_timing_config_t{500000}
#define TWAI_FILTER_CONFIG_ACCEPT_ALL()                                                                                \
    twai_filter_config_t                                                                                               \
    {                                                                                                                  \
        true                                                                                                           \
    }

esp_err_t twai_driver_install(const twai_general_config_t* general, const twai_timing_config_t* timing,
                              const twai_filter_config_t* filter);
esp_err_t twai_start();
esp_err_t twai_receive(twai_message_t* message, uint32_t waitTicks);

// Deliberately no transmit API: CAN.cpp must compile as a receive-only consumer.

#endif
