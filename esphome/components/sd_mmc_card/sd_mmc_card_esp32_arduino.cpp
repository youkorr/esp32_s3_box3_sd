#ifdef USE_ESP32_FRAMEWORK_ARDUINO
#include "sd_mmc_card.h"
#include <SD_MMC.h>

namespace esphome {
namespace sd_mmc_card {

static const char *TAG = "sd_mmc_card";

void SdMmc::setup() {
  if (!SD_MMC.begin()) {
    this->init_error_ = ErrorCode::ERR_MOUNT;
    this->mark_failed();
    return;
  }
  this->is_setup_ = true;
  
  #ifdef USE_SENSOR
  this->update_storage_sensors();
  #endif
}

void SdMmc::write_file(const char *path, const uint8_t *buffer, size_t len) {
  File file = SD_MMC.open(path, FILE_WRITE);
  if (!file) {
    ESP_LOGE(TAG, "Failed to open file for writing: %s", path);
    return;
  }
  
  if (file.write(buffer, len) != len) {
    ESP_LOGE(TAG, "Write failed for file: %s", path);
  }
  file.close();
}

void SdMmc::append_file(const char *path, const uint8_t *buffer, size_t len) {
  File file = SD_MMC.open(path, FILE_APPEND);
  if (!file) {
    ESP_LOGE(TAG, "Failed to open file for appending: %s", path);
    return;
  }
  
  if (file.write(buffer, len) != len) {
    ESP_LOGE(TAG, "Append failed for file: %s", path);
  }
  file.close();
}

bool SdMmc::delete_file(const char *path) {
  if (!SD_MMC.remove(path)) {
    ESP_LOGE(TAG, "Failed to delete file: %s", path);
    return false;
  }
  return true;
}

size_t SdMmc::file_size(const char *path) {
  File file = SD_MMC.open(path);
  if (!file) return 0;
  
  size_t size = file.size();
  file.close();
  return size;
}

bool SdMmc::is_directory(const char *path) {
  File file = SD_MMC.open(path);
  if (!file) return false;
  
  bool is_dir = file.isDirectory();
  file.close();
  return is_dir;
}

std::vector<std::string> SdMmc::list_directory(const char *path, uint8_t depth) {
  std::vector<std::string> entries;
  File root = SD_MMC.open(path);
  if (!root) return entries;
  
  File file = root.openNextFile();
  while (file) {
    entries.push_back(file.name());
    file = root.openNextFile();
  }
  return entries;
}

// Streamed file API
int SdMmc::open_file(const char* path, const char* mode) {
  File file = SD_MMC.open(path, mode);
  if (!file) return -1;
  
  int handle = next_file_handle_++;
  open_files_[handle] = {new File(file), file.size()};
  return handle;
}

size_t SdMmc::read_chunk(int handle, uint8_t* buffer, size_t chunk_size) {
  auto it = open_files_.find(handle);
  if (it == open_files_.end()) return 0;
  
  File* file = static_cast<File*>(it->second.file_ptr);
  return file->read(buffer, chunk_size);
}

#ifdef USE_SENSOR
void SdMmc::update_storage_sensors() {
  uint64_t total = SD_MMC.totalBytes();
  uint64_t used = SD_MMC.usedBytes();
  
  if (this->total_space_sensor_) {
    this->total_space_sensor_->publish_state(total / (1024.0 * 1024.0));
  }
  if (this->used_space_sensor_) {
    this->used_space_sensor_->publish_state(used / (1024.0 * 1024.0));
  }
  if (this->free_space_sensor_) {
    this->free_space_sensor_->publish_state((total - used) / (1024.0 * 1024.0));
  }
}
#endif

}  // namespace sd_mmc_card
}  // namespace esphome
#endif
