#pragma once

#include <stdexcept>

#define ASSERT_MSG(condition, message)                                                                                 \
    do {                                                                                                               \
        if (!(condition)) {                                                                                            \
            throw std::runtime_error("firmware assertion failed");                                                     \
        }                                                                                                              \
    } while (false)
