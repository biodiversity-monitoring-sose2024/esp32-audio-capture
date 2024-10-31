#ifndef FILE_UTILS_H
#define FILE_UTILS_H
#include <string>
#include <vector>

/// @brief Ensures a dir path exists
void ensure_base_path_exists(const std::string& base_path) noexcept;

/// @brief Checks if file exists
bool exists(const std::string& path) noexcept;

/// @brief Moves a file within a filesystem
void move_file(const std::string& from, const std::string& to) noexcept;

/// @brief Gets the filename from a filepath
std::string get_filename(const std::string& path) noexcept;

/// @brief Gets the filename from a path without the extension
std::string get_filename_no_ext(const std::string& path) noexcept;

/// @brief Gets all files in a directory
std::vector<std::string> get_files(const std::string& dir_path) noexcept;

#endif //FILE_UTILS_H

