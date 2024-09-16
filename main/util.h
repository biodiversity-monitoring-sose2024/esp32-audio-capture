#ifndef UTIL_H
#define UTIL_H
#include <cstdint>

inline int64_t get_time() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + (tv.tv_usec / 1000LL / 1000LL);
}

#endif