#ifndef QUEUE_ENTRY_T
#define QUEUE_ENTRY_T

/// @brief A send queue entry
typedef struct send_queue_entry_t {
    /// @brief The request to queue
    std::shared_ptr<payload_t> request;

    /// @brief If it's a data request, the filename of the file to send
    std::string filename = "";

    /// @brief Callback to call if the request did not succeed
    /// the returned bool will indicate if the request should be retried
    std::function<bool(const send_queue_entry_t*, const payload_type_t&)> on_error = [](const send_queue_entry_t* queue_entry, const payload_type_t& type){ return true; };

    /// @brief Callback to call if the request succeeded with an ACK
    std::function<void(const send_queue_entry_t*)> on_success = [](const send_queue_entry_t* queue_entry){};

    /// @brief A list of response types to handle in a special manner.
    /// Any response type other than BLOCKED, RESET, and ACK need to be defined here if they're expected,
    /// else the request will be seen as not successful and on_error will be called.
    /// Payloads defined here need to be able to be responded to with an ACK or RESET!
    std::vector<payload_type_t> custom_handling_payloads = { };

    /// @brief The function to call when a custom payload type is encountered, the returned bool indicates if an ACK or a RESET should be returned to the sender.
    std::function<bool(const send_queue_entry_t*, payload_t*)> custom_callback = [](const send_queue_entry_t* queue_entry, payload_t* payload){ return true; };
} send_queue_entry_t;

#endif