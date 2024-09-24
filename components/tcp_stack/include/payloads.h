#ifndef PAYLOADS_H
#define PAYLOADS_H

#include <cstdint>

/// @brief The types of payloads possible
typedef enum : uint8_t {
    // Responses

    /// @brief This response is sent if the processing of this particular request failed
    /// the node is still open for new requests, even of the same type.
    RESET       = 0x00,

    /// @brief The request has been processed successfully
    ACK         = 0x01,

    /// @brief The node is currently not available to handle this type of request, a new node should be selected
    BLOCKED     = 0x02,

    /// @brief The response to a config request, contains all necessary config values
    RESP_CONFIG      = 0x03,

    // Requests
    /// @brief Sent at the start of a connection, indicating status of the device and which opcode will follow next
    SESSION     = 0x10,

    /// @brief Indicates a data payload
    DATA        = 0x20,

    /// @brief Indicates a request for config
    REQ_CONFIG      = 0x30,
} payload_type_t;

/// @brief The base type for all messages
typedef struct __attribute__((packed,aligned(1))) payload_t {
    /// @brief The type of the payload
    payload_type_t type;

    /// @brief Constructs a new payload_t, generally should not be used
    /// outside of derived type construction!
    /// @param type The type of the payload
    explicit payload_t(payload_type_t type)
        : type(type)
    {}
} payload_t;

/// @brief A base request, used as a polymorphic base type
typedef struct __attribute__((packed,aligned(1))) request_t : payload_t {
    /// @brief The node id, aka mac
    uint8_t node_id[6]{};

    /// @brief Constructs a new request_t, generally should not be used
    /// outside of derived type construction!
    /// @param type The type of the request
    /// @param node_id_in The node id of the device
    request_t(payload_type_t type, std::array<uint8_t,6> node_id_in)
            : payload_t(type)
    {
        // Populate the c array via a std::array
        for (int i = 0; i < node_id_in.size(); i++) {
            node_id[i] = node_id_in[i];
        }
    }
} request_t;

/// @brief A session request, prerequisite for all other requests
typedef struct __attribute__((packed,aligned(1))) session_request_t : request_t {
    /// @brief The power level of the device in percent
    uint8_t power_level;

    /// @brief The memory usage of the device in percent
    uint8_t memory_usage;

    /// @brief The next payload to be sent
    payload_type_t next_payload;

    /// @brief Constructs a new session request
    /// @param node_id The id of the node, aka mac
    /// @param power_level The powerlevel of the device in percent
    /// @param memory_usage The memory usage of the device in percent
    /// @param next_payload The type of the next payload
    session_request_t(std::array<uint8_t,6> node_id, uint8_t power_level, uint8_t memory_usage, payload_type_t next_payload)
            : request_t(payload_type_t::SESSION, node_id),
              power_level(power_level),
              memory_usage(memory_usage),
              next_payload(next_payload)
    {}
} session_request_t;

/// @brief A data payload request sending certain types of raw data
typedef struct __attribute__((packed,aligned(1))) data_request_t : request_t {
    /// @brief The timestamp the file was created
    uint64_t timestamp;

    /// @brief The type of the data
    data_type_t data_type;

    /// @brief The length of the data in bytes
    uint32_t data_length;

    /// @brief The data of the file
    /// generally nullptr if sending since the ESP does not have enough memory to store an entire file in heap
    ///
    /// The file should be accessed through other means when sending
    uint8_t* data;

    /// @brief Constructs a new data request
    /// @param node_id The id of the node, aka mac
    /// @param timestamp The time the file was created
    /// @param data_type The type of the data
    /// @param data_length The length of the data, usually not set when sending, should be set at time of actual sending
    /// @param data The raw data, usually not set when sending, since the file should be provided through other low memory apis
    data_request_t(std::array<uint8_t,6> node_id, uint64_t timestamp, data_type_t data_type, uint32_t data_length = 0, uint8_t* data = nullptr)
            : request_t(payload_type_t::DATA, node_id),
              timestamp(timestamp),
              data_type(data_type),
              data_length(data_length),
              data(data)
    {}
} data_request_t;

/// @brief A config request
typedef struct __attribute__((packed,aligned(1))) config_request_t : request_t {

    /// @brief Constructs a new config request
    /// @param node_id The id of the node, aka mac
    explicit config_request_t(std::array<uint8_t, 6> node_id)
            : request_t(payload_type_t::REQ_CONFIG, node_id)
    {}
} config_request_t;

/// @brief A base response, used as a polymorphic base type
typedef struct __attribute__((packed,aligned(1))) response_t : payload_t {
    /// @brief Constructs a new response_t, generally should not be used
    /// outside of derived type construction!
    /// @param type The type of the payload
    explicit response_t(payload_type_t type)
            : payload_t(type)
    {}
} response_t;

/// @brief A ack response, indicates that a prior request completed successfully
typedef struct __attribute__((packed,aligned(1))) ack_response_t : response_t {

    /// @brief Constructs a new ack response
    ack_response_t()
        : response_t(payload_type_t::ACK)
    {}
} ack_response_t;

/// @brief A reset response, indicates that something went wrong on the other end
/// The other node should still be able to handle the type of request though
typedef struct __attribute__((packed,aligned(1))) reset_response_t : response_t {

    /// @brief Constructs a new reset response
    reset_response_t()
        : response_t(payload_type_t::RESET)
    {}
} reset_response_t;

/// @brief A blocked response, indicates that the node is not able to handle this request right now
/// a time of expected business is attached to this payload.
/// The request should be retried on another node until this time has elapsed
typedef struct __attribute__((packed,aligned(1))) blocked_response_t : response_t {

    /// @brief The expected time of business in seconds
    /// -1 indicates that the node won't be available for the foreseeable future, likely due to low power
    uint16_t expected_time_of_business;

    /// @brief Constructs a new blocked response
    /// @param expected_time_of_business The expected time of business in seconds
    explicit blocked_response_t(int16_t expected_time_of_business)
            : response_t(payload_type_t::BLOCKED)
            , expected_time_of_business(expected_time_of_business)
    {}
} blocked_response_t;

/// @brief A config response
typedef struct __attribute__((packed,aligned(1))) config_response_t : response_t {
    /// @brief Indicates the next time slot in which to start sending
    uint64_t next_timeslot_in;

    /// @brief The length of server addresses
    uint16_t server_addresses_len;

    /// @brief An array of server addresses, each 4 bytes aka uint32_t
    char* server_addresses;

    /// @brief Constructs a new config response
    /// @param next_timeslot_in The next send window
    /// @param server_addresses The server addresses
    config_response_t(int64_t next_timeslot_in, std::vector<char> server_addresses)
            : response_t(payload_type_t::RESP_CONFIG),
              next_timeslot_in(next_timeslot_in),
              server_addresses_len(server_addresses.size()),
              server_addresses(server_addresses.data())
    {}
} config_response_t;

#endif //PAYLOADS_H
