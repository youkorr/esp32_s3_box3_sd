#ifdef USE_ESP_IDF
#include "sd_mmc_card.h"
#include <string>
#include <dirent.h>
#include <sys/stat.h>
#include "esp_vfs_fat.h"

namespace esphome {
namespace sd_mmc_card {

static const char *TAG = "sd_mmc_card";
static const char *BASE_PATH = "/sdcard";

void SdMmc::setup() {
  sdmmc_host_t host = SDMMC_HOST_DEFAULT();
  sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();

  slot_config.clk = (gpio_num_t) this->clk_pin_;
  slot_config.cmd = (gpio_num_t) this->cmd_pin_;
  slot_config.d0 = (gpio_num_t) this->data0_pin_;

  if (!this->mode_1bit_) {
    slot_config.d1 = (gpio_num_t) this->data1_pin_;
    slot_config.d2 = (gpio_num_t) this->data2_pin_;
    slot_config.d3 = (gpio_num_t) this->data3_pin_;
  }

  esp_vfs_fat_sdmmc_mount_config_t mount_config = {
      .format_if_mount_failed = false,
      .max_files = 5,
      .allocation_unit_size = 16 * 1024};

  sdmmc_card_t *card;
  esp_err_t ret = esp_vfs_fat_sdmmc_mount(BASE_PATH, &host, &slot_config, &mount_config, &card);

  if (ret != ESP_OK) {
    this->init_error_ = (ret == ESP_FAIL) ? ErrorCode::ERR_MOUNT : ErrorCode::ERR_NO_CARD;
    this->mark_failed();
    return;
  }
  this->is_setup_ = true;
}

void SdMmc::write_file(const char *path, const uint8_t *buffer, size_t len) {
  std::string full_path = this->build_path(path);
  FILE *file = fopen(full_path.c_str(), "wb");
  if (!file) {
    ESP_LOGE(TAG, "Failed to open file for writing: %s", path);
    return;
  }
  
  if (fwrite(buffer, 1, len, file) != len) {
    ESP_LOGE(TAG, "Write failed for file: %s", path);
  }
  fclose(file);
}

size_t SdMmc::file_size(const char *path) {
  struct stat st;
  if (stat(this->build_path(path).c_str(), &st) == 0) {
    return st.st_size;
  }
  return 0;
}

bool SdMmc::delete_file(const char *path) {
  std::string full_path = this->build_path(path);
  if (remove(full_path.c_str()) != 0) {
    ESP_LOGE(TAG, "Failed to delete file: %s (%s)", path, strerror(errno));
    return false;
  }
  return true;
}

std::vector<std::string> SdMmc::list_directory(const char *path, uint8_t depth) {
  std::vector<std::string> entries;
  DIR *dir = opendir(this->build_path(path).c_str());
  if (!dir) return entries;
  
  struct dirent *entry;
  while ((entry = readdir(dir)) != NULL) {
    if (entry->d_type == DT_REG || entry->d_type == DT_DIR) {
      entries.push_back(entry->d_name);
    }
  }
  closedir(dir);
  return entries;
}

// Streamed file API
int SdMmc::open_file(const char* path, const char* mode) {
  std::string full_path = this->build_path(path);
  FILE* file = fopen(full_path.c_str(), mode);
  if (!file) return -1;
  
  int handle = next_file_handle_++;
  open_files_[handle] = {file, this->file_size(path)};
  return handle;
}

size_t SdMmc::read_chunk(int handle, uint8_t* buffer, size_t chunk_size) {
  auto it = open_files_.find(handle);
  if (it == open_files_.end()) return 0;
  
  FILE* file = static_cast<FILE*>(it->second.file_ptr);
  return fread(buffer, 1, chunk_size, file);
}

#ifdef USE_SENSOR
void SdMmc::update_storage_sensors() {
  FATFS *fs;
  DWORD free_clusters;
  FRESULT res = f_getfree("", &free_clusters, &fs);
  
  if (res != FR_OK) {
    ESP_LOGE(TAG, "Failed to get storage info: %d", res);
    return;
  }
  
  uint64_t total = (fs->n_fatent - 2) * fs->csize * 512;
  uint64_t free = free_clusters * fs->csize * 512;
  
  if (this->total_space_sensor_) {
    this->total_space_sensor_->publish_state(total / (1024.0 * 1024.0));
  }
  if (this->free_space_sensor_) {
    this->free_space_sensor_->publish_state(free / (1024.0 * 1024.0));
  }
  if (this->used_space_sensor_) {
    this->used_space_sensor_->publish_state((total - free) / (1024.0 * 1024.0));
  }
}
#endif

}  // namespace sd_mmc_card
}  // namespace esphome
#endif

