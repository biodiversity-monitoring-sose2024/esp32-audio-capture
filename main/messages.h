#ifndef MESSAGES_H
#define MESSAGES_H
#include <cstdint>
#include <array>
// OPCODES
typedef enum : uint8_t {
    RESET           = 0x00,
    ACK             = 0x01,
    BLOCKED         = 0x02,
    RESP_CONFIG          = 0x03
} response_type_t;

typedef enum : uint8_t {
    SESSION         = 0x10,
    DATA            = 0x20,
    REQ_CONFIG          = 0x30,
} request_type_t;

typedef enum {
    WAV             = 0x01,
    CSV             = 0x10,
} data_type_t;
/*
    Response type structs
*/

typedef struct response_t {
    response_type_t type;
    response_t(response_type_t type)
        : type(type)
        {}
} response_t;

typedef struct ack_response_t : response_t {
    ack_response_t()
        : response_t(response_type_t::ACK)
        {}
} ack_response_t;

typedef struct reset_response_t : response_t {
    reset_response_t()
        : response_t(response_type_t::RESET)
        {}
} reset_response_t;

typedef struct blocked_response_t : response_t {
    int16_t expected_time_of_business;
    blocked_response_t(int16_t expected_time_of_business) 
        : response_t(response_type_t::BLOCKED)
        , expected_time_of_business(expected_time_of_business)
    {}
} blocked_response_t;

typedef struct config_response_t : response_t {
    // UNIX ms timestamp
    int64_t next_timeslot_in;
    uint16_t server_addresses_len;
    uint32_t* server_addresses;
    config_response_t(int64_t next_timeslot_in, std::vector<uint32_t> server_addresses)
        : response_t(response_type_t::RESP_CONFIG),
        next_timeslot_in(next_timeslot_in),
        server_addresses_len(server_addresses.size()),
        server_addresses(server_addresses.data())
    {}
} config_response_t;

/*
    Request type structs
*/

typedef struct request_t {
    request_type_t type;
    uint8_t node_id[6]; 

    request_t(request_type_t type, std::array<uint8_t,6> node_id_in)
        : type(type)
        {
            for (int i = 0; i < node_id_in.size(); i++) {
                node_id[i] = node_id_in[i];
            }            
        }
} request_t;

typedef struct session_request_t : request_t {
    uint8_t power_level;
    uint8_t memory_usage;
    request_type_t next_request;

    session_request_t(std::array<uint8_t,6> node_id, uint8_t power_level, uint8_t memory_usage, request_type_t next_request) 
        : request_t(request_type_t::SESSION, node_id),
        power_level(power_level),
        memory_usage(memory_usage),
        next_request(next_request)
    {}
} session_request_t;

typedef struct data_request_t : request_t {
    uint64_t timestamp;
    data_type_t data_type;
    uint32_t data_length;
    uint8_t* data;

    data_request_t(std::array<uint8_t,6> node_id, uint64_t timestamp, data_type_t data_type, uint32_t data_length, std::vector<uint8_t> data)
        : request_t(request_type_t::DATA, node_id),
        timestamp(timestamp),
        data_type(data_type),
        data_length(data_length),
        data(data.data())
    {}
} data_request_t;

typedef struct config_request_t : request_t {
    config_request_t(std::array<uint8_t, 6> node_id)
        : request_t(request_type_t::REQ_CONFIG, node_id)
    {}
} config_request_t;
#endif