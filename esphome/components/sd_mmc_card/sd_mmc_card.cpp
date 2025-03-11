#include "sd_mmc_card.h"

#include <algorithm>
#include <math.h>
#include "esphome/core/log.h"

namespace esphome {
namespace sd_mmc_card {

static const char *TAG = "sd_mmc_card";

void SdMmc::setup() {
  if (this->power_ctrl_pin_ != -1) {
    pinMode(this->power_ctrl_pin_, OUTPUT);
    digitalWrite(this->power_ctrl_pin_, HIGH);
  }

  this->init_error_ = NO_ERROR;
}

void SdMmc::loop() {
  if (this->init_error_ != NO_ERROR) {
    return;
  }

  this->update_sensors();
}

void SdMmc::write_file(const char *path, const uint8_t *buffer, size_t len, const char *mode) {
  FILE *file = fopen(path, mode);
  if (!file) {
    ESP_LOGE(TAG, "Failed to open file for writing");
    return;
  }

  size_t bytes_written = fwrite(buffer, 1, len, file);
  fclose(file);

  if (bytes_written != len) {
    ESP_LOGE(TAG, "Failed to write complete file (wrote %zu/%zu bytes)", bytes_written, len);
  }
}

bool SdMmc::delete_file(const char *path) {
  if (remove(path) != 0) {
    ESP_LOGE(TAG, "Failed to delete file");
    return false;
  }
  return true;
}

bool SdMmc::create_directory(const char *path) {
  if (mkdir(path, 0755) != 0) {
    ESP_LOGE(TAG, "Failed to create directory");
    return false;
  }
  return true;
}

bool SdMmc::remove_directory(const char *path) {
  if (rmdir(path) != 0) {
    ESP_LOGE(TAG, "Failed to remove directory");
    return false;
  }
  return true;
}

bool SdMmc::is_directory(const char *path) {
  struct stat st;
  if (stat(path, &st) != 0) {
    return false;
  }
  return S_ISDIR(st.st_mode);
}

std::vector<std::string> SdMmc::list_directory(const char *path, uint8_t depth) {
  std::vector<std::string> result;
  DIR *dir = opendir(path);
  if (!dir) {
    return result;
  }

  struct dirent *entry;
  while ((entry = readdir(dir)) != NULL) {
    std::string entry_name = entry->d_name;
    if (entry_name == "." || entry_name == "..") {
      continue;
    }

    result.push_back(entry_name);
    if (entry->d_type == DT_DIR && depth > 0) {
      std::string subdir_path = std::string(path) + "/" + entry_name;
      std::vector<std::string> subdir = this->list_directory(subdir_path.c_str(), depth - 1);
      result.insert(result.end(), subdir.begin(), subdir.end());
    }
  }

  closedir(dir);
  return result;
}

std::vector<FileInfo> SdMmc::list_directory_file_info(const char *path, uint8_t depth) {
  std::vector<FileInfo> result;
  this->list_directory_file_info_rec(path, depth, result);
  return result;
}

std::vector<FileInfo> &SdMmc::list_directory_file_info_rec(const char *path, uint8_t depth, std::vector<FileInfo> &list) {
  DIR *dir = opendir(path);
  if (!dir) {
    return list;
  }

  struct dirent *entry;
  while ((entry = readdir(dir)) != NULL) {
    std::string entry_name = entry->d_name;
    if (entry_name == "." || entry_name == "..") {
      continue;
    }

    std::string full_path = std::string(path) + "/" + entry_name;
    struct stat st;
    if (stat(full_path.c_str(), &st) == 0) {
      list.emplace_back(entry_name, st.st_size, S_ISDIR(st.st_mode));
      if (S_ISDIR(st.st_mode) && depth > 0) {
        this->list_directory_file_info_rec(full_path.c_str(), depth - 1, list);
      }
    }
  }

  closedir(dir);
  return list;
}

size_t SdMmc::file_size(const char *path) {
  struct stat st;
  if (stat(path, &st) != 0) {
    return 0;
  }
  return st.st_size;
}

void SdMmc::update_sensors() {
#ifdef USE_SENSOR
  if (this->used_space_sensor_ != nullptr) {
    this->used_space_sensor_->publish_state(this->get_used_space());
  }
  if (this->total_space_sensor_ != nullptr) {
    this->total_space_sensor_->publish_state(this->get_total_space());
  }
  if (this->free_space_sensor_ != nullptr) {
    this->free_space_sensor_->publish_state(this->get_free_space());
  }
#endif

#ifdef USE_TEXT_SENSOR
  if (this->sd_card_type_sensor_ != nullptr) {
    this->sd_card_type_sensor_->publish_state(this->get_sd_card_type());
  }
#endif
}

}  // namespace sd_mmc_card
}  // namespace esphome

