#pragma once

#include <iostream>
#include <time.h>
#include <exception>

#define THROW(...)                                                                             \
    do                                                                                         \
    {                                                                                          \
        time_t t;                                                                              \
        time(&t);                                                                              \
        char tmp[32] = {0};                                                                    \
        strftime(tmp, sizeof(tmp), "%Y-%m-%d_%H:%M:%S", localtime(&t));                        \
        char buff[4096];                                                                       \
        auto a = snprintf(buff, 4096, "%s %s:%d:%s: ", tmp, __FILE__, __LINE__, __FUNCTION__); \
        auto b = snprintf(buff + a, 4096 - a, __VA_ARGS__);                                    \
        snprintf(buff + a + b, 4096 - a - b, "\n");                                            \
    } while (0);
