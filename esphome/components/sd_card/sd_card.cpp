#include "sd_card.h"
#include <algorithm>
#include "esphome/core/log.h"
#include "FS.h"
#include "SD_MMC.h"

namespace esphome {
namespace sd_card {

static const char *TAG = "sd_card";

void SdMmc::setup() {
  if (!SD_MMC.begin()) {
    this->mark_failed();
    ESP_LOGE(TAG, "Failed to mount SD card");
    return;
  }
  ESP_LOGI(TAG, "SD card mounted successfully");
}

void SdMmc::loop() {}

void SdMmc::dump_config() {
  ESP_LOGCONFIG(TAG, "SD MMC Component");
  ESP_LOGCONFIG(TAG, "  Mode 1 bit: %s", this->mode_1bit_ ? "true" : "false");
  ESP_LOGCONFIG(TAG, "  CLK Pin: %d", this->clk_pin_);
  ESP_LOGCONFIG(TAG, "  CMD Pin: %d", this->cmd_pin_);
  ESP_LOGCONFIG(TAG, "  DATA0 Pin: %d", this->data0_pin_);
  if (!this->mode_1bit_) {
    ESP_LOGCONFIG(TAG, "  DATA1 Pin: %d", this->data1_pin_);
    ESP_LOGCONFIG(TAG, "  DATA2 Pin: %d", this->data2_pin_);
    ESP_LOGCONFIG(TAG, "  DATA3 Pin: %d", this->data3_pin_);
  }
}

void SdMmc::write_file(const char *path, const uint8_t *buffer, size_t len, const char *mode) {
  File file = SD_MMC.open(path, mode);
  if (!file) {
    ESP_LOGE(TAG, "Failed to open file for writing: %s", path);
    return;
  }
  file.write(buffer, len);
  file.close();
}

void SdMmc::write_file(const char *path, const uint8_t *buffer, size_t len) {
  this->write_file(path, buffer, len, "w");
}

void SdMmc::append_file(const char *path, const uint8_t *buffer, size_t len) {
  this->write_file(path, buffer, len, "a");
}

std::vector<std::string> SdMmc::list_directory(const char *path, uint8_t depth) {
  std::vector<std::string> list;
  std::vector<FileInfo> infos = this->list_directory_file_info(path, depth);
  std::transform(infos.begin(), infos.end(), std::back_inserter(list), [](FileInfo const &info) { return info.path; });
  return list;
}

std::vector<std::string> SdMmc::list_directory(std::string path, uint8_t depth) {
  return this->list_directory(path.c_str(), depth);
}

std::vector<FileInfo> SdMmc::list_directory_file_info(const char *path, uint8_t depth) {
  std::vector<FileInfo> list;
  this->list_directory_file_info_rec(path, depth, list);
  return list;
}

std::vector<FileInfo> SdMmc::list_directory_file_info(std::string path, uint8_t depth) {
  return this->list_directory_file_info(path.c_str(), depth);
}

void SdMmc::list_directory_file_info_rec(const char *path, uint8_t depth, std::vector<FileInfo> &list) {
  File root = SD_MMC.open(path);
  if (!root) {
    ESP_LOGE(TAG, "Failed to open directory: %s", path);
    return;
  }
  if (!root.isDirectory()) {
    ESP_LOGE(TAG, "Not a directory: %s", path);
    return;
  }

  File file = root.openNextFile();
  while (file) {
    list.emplace_back(file.name(), file.size(), file.isDirectory());
    if (depth > 0 && file.isDirectory()) {
      this->list_directory_file_info_rec(file.name(), depth - 1, list);
    }
    file = root.openNextFile();
  }
  root.close();
}

size_t SdMmc::file_size(std::string const &path) {
  File file = SD_MMC.open(path.c_str());
  if (!file) {
    ESP_LOGE(TAG, "Failed to open file: %s", path.c_str());
    return 0;
  }
  size_t size = file.size();
  file.close();
  return size;
}

bool SdMmc::is_directory(std::string const &path) {
  File file = SD_MMC.open(path.c_str());
  if (!file) {
    ESP_LOGE(TAG, "Failed to open file: %s", path.c_str());
    return false;
  }
  bool is_dir = file.isDirectory();
  file.close();
  return is_dir;
}

bool SdMmc::delete_file(std::string const &path) {
  return SD_MMC.remove(path.c_str());
}

std::vector<uint8_t> SdMmc::read_file(std::string const &path) {
  std::vector<uint8_t> data;
  File file = SD_MMC.open(path.c_str());
  if (!file) {
    ESP_LOGE(TAG, "Failed to open file: %s", path.c_str());
    return data;
  }
  data.resize(file.size());
  file.read(data.data(), data.size());
  file.close();
  return data;
}

std::string SdMmc::error_code_to_string(SdMmc::ErrorCode code) {
  switch (code) {
    case ErrorCode::ERR_PIN_SETUP:
      return "Failed to set pins";
    case ErrorCode::ERR_MOUNT:
      return "Failed to mount card";
    case ErrorCode::ERR_NO_CARD:
      return "No card found";
    default:
      return "Unknown error";
  }
}

FileInfo::FileInfo(std::string const &path, size_t size, bool is_directory)
    : path(path), size(size), is_directory(is_directory) {}

#ifdef USE_SENSOR
void SdMmc::add_file_size_sensor(sensor::Sensor *sensor, std::string const &path) {
  this->file_size_sensors_.emplace_back(sensor, path);
}
#endif

}  // namespace sd_card
}  // namespace esphome

