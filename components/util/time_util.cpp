#include "time_util.h"
#include <ctime>
#include <syncstream>

uint64_t get_time() noexcept {
    struct timeval tv{};
    gettimeofday(&tv, nullptr);
    return tv.tv_sec + (tv.tv_usec / 1000LL / 1000LL);
}