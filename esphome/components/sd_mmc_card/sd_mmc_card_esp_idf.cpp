#include "sd_mmc_card.h"

#ifdef USE_ESP_IDF

#include "math.h"
#include "esphome/core/log.h"
#include "freertos/task.h"
#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
#include "driver/sdmmc_types.h"
#include "driver/gpio.h"

namespace esphome {
namespace sd_mmc_card {

static const char *TAG = "sd_mmc_esp_idf";

void SdMmc::setup() {
  if (this->power_ctrl_pin_ != nullptr)
    this->power_ctrl_pin_->setup();

  sdmmc_host_t host = SDMMC_HOST_DEFAULT();
  sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();

  slot_config.clk = static_cast<gpio_num_t>(this->clk_pin_);
  slot_config.cmd = static_cast<gpio_num_t>(this->cmd_pin_);
  slot_config.d0 = static_cast<gpio_num_t>(this->data0_pin_);

  if (!this->mode_1bit_) {
    slot_config.d1 = static_cast<gpio_num_t>(this->data1_pin_);
    slot_config.d2 = static_cast<gpio_num_t>(this->data2_pin_);
    slot_config.d3 = static_cast<gpio_num_t>(this->data3_pin_);
  }

  esp_vfs_fat_sdmmc_mount_config_t mount_config = {
      .format_if_mount_failed = false,
      .max_files = 5,
      .allocation_unit_size = 16 * 1024};

  sdmmc_card_t *card;
  esp_err_t ret = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot_config, &mount_config, &card);

  if (ret != ESP_OK) {
    if (ret == ESP_FAIL) {
      ESP_LOGE(TAG, "Failed to mount filesystem");
      this->init_error_ = ErrorCode::ERR_MOUNT;
    } else {
      ESP_LOGE(TAG, "Failed to initialize the card (%s)", esp_err_to_name(ret));
      this->init_error_ = ErrorCode::ERR_NO_CARD;
    }
    this->mark_failed();
    return;
  }

  this->card_ = card;
  update_sensors();
}

void SdMmc::write_file(const char *path, const uint8_t *buffer, size_t len, const char *mode) {
  std::string full_path = build_path(path);
  FILE *file = fopen(full_path.c_str(), mode);
  if (!file) {
    ESP_LOGE(TAG, "Failed to open file for writing");
    return;
  }

  fwrite(buffer, 1, len, file);
  fclose(file);
  this->update_sensors();
}

bool SdMmc::create_directory(const char *path) {
  std::string full_path = build_path(path);
  ESP_LOGV(TAG, "Create directory: %s", full_path.c_str());
  if (mkdir(full_path.c_str(), 0755) {
    ESP_LOGE(TAG, "Failed to create directory");
    return false;
  }
  this->update_sensors();
  return true;
}

bool SdMmc::remove_directory(const char *path) {
  std::string full_path = build_path(path);
  ESP_LOGV(TAG, "Remove directory: %s", full_path.c_str());
  if (rmdir(full_path.c_str())) {
    ESP_LOGE(TAG, "Failed to remove directory");
    return false;
  }
  this->update_sensors();
  return true;
}

bool SdMmc::delete_file(const char *path) {
  std::string full_path = build_path(path);
  ESP_LOGV(TAG, "Delete File: %s", full_path.c_str());
  if (remove(full_path.c_str())) {
    ESP_LOGE(TAG, "Failed to delete file");
    return false;
  }
  this->update_sensors();
  return true;
}

std::vector<uint8_t> SdMmc::read_file(char const *path) {
  std::string full_path = build_path(path);
  ESP_LOGV(TAG, "Read File: %s", full_path.c_str());
  FILE *file = fopen(full_path.c_str(), "rb");
  if (!file) {
    ESP_LOGE(TAG, "Failed to open file for reading");
    return std::vector<uint8_t>();
  }

  fseek(file, 0, SEEK_END);
  size_t size = ftell(file);
  fseek(file, 0, SEEK_SET);

  std::vector<uint8_t> buffer(size);
  fread(buffer.data(), 1, size, file);
  fclose(file);

  return buffer;
}

std::vector<FileInfo> &SdMmc::list_directory_file_info_rec(const char *path, uint8_t depth,
                                                           std::vector<FileInfo> &list) {
  std::string full_path = build_path(path);
  ESP_LOGV(TAG, "Listing directory file info: %s", full_path.c_str());

  DIR *dir = opendir(full_path.c_str());
  if (!dir) {
    ESP_LOGE(TAG, "Failed to open directory");
    return list;
  }

  struct dirent *entry;
  while ((entry = readdir(dir)) != nullptr) {
    std::string entry_path = full_path + "/" + entry->d_name;
    struct stat stat_buf;
    if (stat(entry_path.c_str(), &stat_buf) == 0) {
      list.emplace_back(entry_path, stat_buf.st_size, S_ISDIR(stat_buf.st_mode));
      if (S_ISDIR(stat_buf.st_mode) && depth > 0) {
        list_directory_file_info_rec(entry_path.c_str(), depth - 1, list);
      }
    }
  }

  closedir(dir);
  return list;
}

bool SdMmc::is_directory(const char *path) {
  std::string full_path = build_path(path);
  struct stat stat_buf;
  if (stat(full_path.c_str(), &stat_buf) != 0) {
    ESP_LOGE(TAG, "Failed to stat path");
    return false;
  }
  return S_ISDIR(stat_buf.st_mode);
}

size_t SdMmc::file_size(const char *path) {
  std::string full_path = build_path(path);
  struct stat stat_buf;
  if (stat(full_path.c_str(), &stat_buf) != 0) {
    ESP_LOGE(TAG, "Failed to stat path");
    return 0;
  }
  return stat_buf.st_size;
}

std::string SdMmc::sd_card_type() const {
  if (!this->card_) {
    return "NONE";
  }

  switch (this->card_->ocr & SD_OCR_CARD_CAPACITY) {
    case SD_OCR_CARD_CAPACITY_SDHC:
      return "SDHC";
    case SD_OCR_CARD_CAPACITY_SDSC:
      return "SDSC";
    default:
      return "UNKNOWN";
  }
}

void SdMmc::update_sensors() {
#ifdef USE_SENSOR
  if (!this->card_) {
    return;
  }

  FATFS *fs;
  DWORD free_clusters;
  if (f_getfree("0:", &free_clusters, &fs) != FR_OK) {
    ESP_LOGE(TAG, "Failed to get free space");
    return;
  }

  uint64_t total_bytes = (uint64_t)(fs->csize) * (fs->n_fatent - 2) * (fs->ssize);
  uint64_t used_bytes = total_bytes - ((uint64_t)free_clusters * fs->csize * fs->ssize);

  if (this->used_space_sensor_ != nullptr)
    this->used_space_sensor_->publish_state(used_bytes);
  if (this->total_space_sensor_ != nullptr)
    this->total_space_sensor_->publish_state(total_bytes);
  if (this->free_space_sensor_ != nullptr)
    this->free_space_sensor_->publish_state(total_bytes - used_bytes);

  for (auto &sensor : this->file_size_sensors_) {
    if (sensor.sensor != nullptr)
      sensor.sensor->publish_state(this->file_size(sensor.path));
  }
#endif
}

}  // namespace sd_mmc_card
}  // namespace esphome

#endif  // USE_ESP_IDF

