#include "file_utils.h"

#include <filesystem>
#include <cstring>
#include <dirent.h>
#include <esp_log.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/unistd.h>

void ensure_base_path_exists(const std::string& base_path) noexcept {
    if (DIR *dir = opendir(base_path.c_str())) {
        closedir(dir);
        return;
    }

    if (ENOENT != errno) {
        const int err = errno;
        ESP_LOGE("ensure_base_path_exists", "Failed to check if directory %s exists (%d): %s", base_path.c_str(), err, strerror(err));
        return;
    }

    ESP_LOGD("ensure_base_path_exists", "Creating directory %s", base_path.c_str());
    if (mkdir(base_path.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == -1) {
        const int err = errno;
        ESP_LOGE("ensure_base_path_exists", "Failed to create directory %s (%d): %s", base_path.c_str(), err, strerror(err));
    }
}

bool exists(const std::string &path) noexcept {
    return access(path.c_str(), F_OK) == 0;
}

void move_file(const std::string &from, const std::string &to) noexcept {
    if (!exists(from)) {
        ESP_LOGE("move_file", "Cannot access origin file %s, not moving!", from.c_str());
        return;
    }

    if (exists(to)) {
        ESP_LOGW("move_file", "Destination file %s exists, removing already present file!", to.c_str());
        if (remove(to.c_str()) == -1) {
            const int err = errno;
            ESP_LOGE("move_file", "Failed to remove file %s (%d): %s", to.c_str(), err, strerror(err));
            return;
        }
    }

    if (rename(from.c_str(), to.c_str()) == -1) {
        const int err = errno;
        ESP_LOGE("move_file", "Error moving file %s to %s (%d): %s", from.c_str(), to.c_str(), err, strerror(err));
    }
}

std::string get_filename(const std::string &path) noexcept {
    return path.substr(path.find_last_of("/\\") + 1);
}

std::string get_filename_no_ext(const std::string &path) noexcept {
    std::filesystem::path stdpath(path);
    return stdpath.stem();
}

std::vector<std::string> get_files(const std::string &dir_path) noexcept {
    std::vector<std::string> files {};

    DIR *dir;
    dirent *ent;
    if ((dir = opendir(dir_path.c_str())) == nullptr) {
        ESP_LOGE("get_files", "Could not open directory %s", dir_path.c_str());
        return files;
    }

    while ((ent = readdir(dir)) != nullptr) {
        if (ent->d_type == DT_DIR) {
            ESP_LOGD("get_files", "Found directory %s, skipping", ent->d_name);
            continue;
        }

        ESP_LOGD("get_files", "Found file %s", ent->d_name);
        files.emplace_back(ent->d_name);
    }
    closedir(dir);

    return files;
}
