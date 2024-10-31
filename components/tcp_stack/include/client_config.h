#ifndef CLIENT_CONFIG_H
#define CLIENT_CONFIG_H

typedef struct {
  /// @brief The mac address of the device as a byte array
  std::array<uint8_t, 6> mac;

  /// @brief The directory in which to look for new files to send
  std::string file_dir;

  /// @brief The directory which is used for storing files that are enqueued to be sent
  std::string queue_dir;

  /// @brief Indicates whether to delete a file after sending it successfully
  bool delete_after_send = true;
} client_config_t;

#endif //CLIENT_CONFIG_H
