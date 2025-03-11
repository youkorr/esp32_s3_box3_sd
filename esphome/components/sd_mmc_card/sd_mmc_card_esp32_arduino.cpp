#include "sd_mmc_card.h"

#ifdef USE_ESP32_FRAMEWORK_ARDUINO

#include "math.h"
#include "esphome/core/log.h"
#include "SD_MMC.h"
#include "FS.h"

#define SDCARD_PWR_CTRL GPIO_NUM_43

namespace esphome {
namespace sd_mmc_card {

static const char *TAG = "sd_mmc_card_esp32_arduino";

void SdMmc::setup() {
  if (!SD_MMC.begin()) {
    this->init_error_ = ERR_MOUNT;
    ESP_LOGE(TAG, "Failed to mount SD card");
    return;
  }

  uint8_t cardType = SD_MMC.cardType();
  if (cardType == CARD_NONE) {
    this->init_error_ = ERR_NO_CARD;
    ESP_LOGE(TAG, "No SD card attached");
    return;
  }

  ESP_LOGI(TAG, "SD card mounted successfully");
  this->init_error_ = ErrorCode::NO_ERROR;
}

void SdMmc::loop() {
  if (this->init_error_ != ErrorCode::NO_ERROR) {
    return;
  }

  this->update_sensors();
}

void SdMmc::dump_config() {
  ESP_LOGCONFIG(TAG, "SD/MMC Card:");
  if (this->init_error_ != ErrorCode::NO_ERROR) {
    ESP_LOGE(TAG, "  Error: %s", error_code_to_string(this->init_error_).c_str());
    return;
  }

  ESP_LOGCONFIG(TAG, "  Card Type: %s", this->sd_card_type_to_string(SD_MMC.cardType()).c_str());
  ESP_LOGCONFIG(TAG, "  Total Size: %.2f MB", SD_MMC.totalBytes() / (1024.0 * 1024.0));
  ESP_LOGCONFIG(TAG, "  Used Space: %.2f MB", SD_MMC.usedBytes() / (1024.0 * 1024.0));
}

bool SdMmc::read_file_chunk(const char *path, uint8_t *buffer, size_t offset, size_t len) {
  File file = SD_MMC.open(path);
  if (!file) {
    ESP_LOGE(TAG, "Failed to open file for reading");
    return false;
  }

  if (!file.seek(offset)) {
    ESP_LOGE(TAG, "Failed to seek to position %zu", offset);
    file.close();
    return false;
  }

  size_t bytesRead = file.read(buffer, len);
  file.close();
  
  if (bytesRead != len) {
    ESP_LOGE(TAG, "Failed to read %zu bytes (read %zu)", len, bytesRead);
    return false;
  }

  return true;
}

bool SdMmc::read_file_by_chunks(const char *path, void (*callback)(const uint8_t *, size_t), size_t chunk_size) {
  File file = SD_MMC.open(path);
  if (!file) {
    ESP_LOGE(TAG, "Failed to open file for reading");
    return false;
  }

  size_t fileSize = file.size();
  file.close();

  if (fileSize == 0) {
    ESP_LOGW(TAG, "File is empty");
    return true;
  }

  size_t offset = 0;
  uint8_t buffer[chunk_size];

  while (offset < fileSize) {
    size_t remaining = fileSize - offset;
    size_t chunk_len = (remaining < chunk_size) ? remaining : chunk_size;

    if (!read_file_chunk(path, buffer, offset, chunk_len)) {
      ESP_LOGE(TAG, "Error reading chunk starting at offset %zu", offset);
      return false;
    }

    callback(buffer, chunk_len);

    offset += chunk_len;
  }

  return true;
}

std::vector<uint8_t> SdMmc::read_file(const char *path) {
  std::vector<uint8_t> data;
  File file = SD_MMC.open(path);
  if (!file) {
    ESP_LOGE(TAG, "Failed to open file for reading");
    return data;
  }

  size_t size = file.size();
  data.resize(size);
  file.read(data.data(), size);
  file.close();
  return data;
}

void SdMmc::write_file(const char *path, const uint8_t *buffer, size_t len) {
  File file = SD_MMC.open(path, FILE_WRITE);
  if (!file) {
    ESP_LOGE(TAG, "Failed to open file for writing");
    return;
  }

  size_t bytesWritten = file.write(buffer, len);
  file.close();

  if (bytesWritten != len) {
    ESP_LOGE(TAG, "Failed to write complete file (wrote %zu/%zu bytes)", bytesWritten, len);
  }
}

void SdMmc::append_file(const char *path, const uint8_t *buffer, size_t len) {
  File file = SD_MMC.open(path, FILE_APPEND);
  if (!file) {
    ESP_LOGE(TAG, "Failed to open file for appending");
    return;
  }

  size_t bytesWritten = file.write(buffer, len);
  file.close();

  if (bytesWritten != len) {
    ESP_LOGE(TAG, "Failed to append complete data (wrote %zu/%zu bytes)", bytesWritten, len);
  }
}

bool SdMmc::delete_file(const char *path) {
  if (!SD_MMC.remove(path)) {
    ESP_LOGE(TAG, "Failed to delete file");
    return false;
  }
  return true;
}

bool SdMmc::create_directory(const char *path) {
  if (!SD_MMC.mkdir(path)) {
    ESP_LOGE(TAG, "Failed to create directory");
    return false;
  }
  return true;
}

bool SdMmc::remove_directory(const char *path) {
  if (!SD_MMC.rmdir(path)) {
    ESP_LOGE(TAG, "Failed to remove directory");
    return false;
  }
  return true;
}

bool SdMmc::is_directory(const char *path) {
  File file = SD_MMC.open(path);
  if (!file) {
    return false;
  }
  bool is_dir = file.isDirectory();
  file.close();
  return is_dir;
}

std::vector<std::string> SdMmc::list_directory(const char *path, uint8_t depth) {
  std::vector<std::string> result;
  File root = SD_MMC.open(path);
  if (!root) {
    return result;
  }

  File file = root.openNextFile();
  while (file) {
    result.push_back(file.name());
    if (file.isDirectory() && depth > 0) {
      std::vector<std::string> subdir = this->list_directory(file.name(), depth - 1);
      result.insert(result.end(), subdir.begin(), subdir.end());
    }
    file = root.openNextFile();
  }

  root.close();
  return result;
}

std::vector<FileInfo> SdMmc::list_directory_file_info(const char *path, uint8_t depth) {
  std::vector<FileInfo> result;
  this->list_directory_file_info_rec(path, depth, result);
  return result;
}

std::vector<FileInfo> &SdMmc::list_directory_file_info_rec(const char *path, uint8_t depth, std::vector<FileInfo> &list) {
  File root = SD_MMC.open(path);
  if (!root) {
    return list;
  }

  File file = root.openNextFile();
  while (file) {
    list.emplace_back(file.name(), file.size(), file.isDirectory());
    if (file.isDirectory() && depth > 0) {
      this->list_directory_file_info_rec(file.name(), depth - 1, list);
    }
    file = root.openNextFile();
  }

  root.close();
  return list;
}

size_t SdMmc::file_size(const char *path) {
  File file = SD_MMC.open(path);
  if (!file) {
    return 0;
  }
  size_t size = file.size();
  file.close();
  return size;
}

std::string SdMmc::sd_card_type_to_string(int type) const {
  switch (type) {
    case CARD_MMC: return "MMC";
    case CARD_SD: return "SDSC";
    case CARD_SDHC: return "SDHC";
    default: return "Unknown";
  }
}

void SdMmc::update_sensors() {
#ifdef USE_SENSOR
  if (this->used_space_sensor_ != nullptr) {
    this->used_space_sensor_->publish_state(SD_MMC.usedBytes() / (1024.0 * 1024.0));
  }
  if (this->total_space_sensor_ != nullptr) {
    this->total_space_sensor_->publish_state(SD_MMC.totalBytes() / (1024.0 * 1024.0));
  }
  if (this->free_space_sensor_ != nullptr) {
    this->free_space_sensor_->publish_state((SD_MMC.totalBytes() - SD_MMC.usedBytes()) / (1024.0 * 1024.0));
  }
#endif

#ifdef USE_TEXT_SENSOR
  if (this->sd_card_type_sensor_ != nullptr) {
    this->sd_card_type_sensor_->publish_state(this->sd_card_type_to_string(SD_MMC.cardType()));
  }
#endif
}

std::string SdMmc::error_code_to_string(ErrorCode error) {
  switch (error) {
    case ERR_PIN_SETUP: return "Pin setup error";
    case ERR_MOUNT: return "Mount error";
    case ERR_NO_CARD: return "No card present";
    default: return "No error";
  }
}

}  // namespace sd_mmc_card
}  // namespace esphome

#endif  // USE_ESP32_FRAMEWORK_ARDUINO
