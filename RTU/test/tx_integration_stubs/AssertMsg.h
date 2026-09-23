#pragma once
#include <stdexcept>
#define ASSERT_MSG(condition, message)                                                                                 \
    do {                                                                                                               \
        if (!(condition))                                                                                              \
            throw std::runtime_error(message);                                                                         \
    } while (false)
