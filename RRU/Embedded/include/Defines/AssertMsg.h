#ifndef ASSERT_MSG_H
#define ASSERT_MSG_H

#define ASSERT_MSG(condition, msg)          \
    do {                                    \
        if (!(condition)) {                 \
            Serial.println(msg);            \
            Serial.flush();                 \
            abort();                        \
        }                                   \
    } while(0)

#endif // ASSERT_MSG_H