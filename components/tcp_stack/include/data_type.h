#ifndef DATA_TYPE_H
#define DATA_TYPE_H

#include <cstdint>

/// @brief Indicates which kind of data is included in a data request payload
typedef enum : uint8_t {
    WAV = 0x01,
    CSV = 0x10,
} data_type_t;

#endif