#include "sd_mmc_card.h"

#ifdef USE_ESP_IDF
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"
#include "driver/sdmmc_defs.h"
#include "sdmmc_cmd.h"

namespace esphome {
namespace sd_mmc_card {

static const char *TAG = "sd_mmc_card";

void SdMmc::setup() {
  if (this->power_ctrl_pin_ != nullptr) {
    gpio_config_t io_conf = {
        .pin_bit_mask = static_cast<uint64_t>(this->power_ctrl_pin_->get_pin()),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE};
    gpio_config(&io_conf);
    gpio_set_level(static_cast<gpio_num_t>(this->power_ctrl_pin_->get_pin()), 0);
    ESP_LOGI(TAG, "SD Card power control enabled on GPIO %d", this->power_ctrl_pin_->get_pin());
  }

  esp_vfs_fat_sdmmc_mount_config_t mount_config = {
      .format_if_mount_failed = false,
      .max_files = 5,
      .allocation_unit_size = 16 * 1024};

  sdmmc_host_t host = SDMMC_HOST_DEFAULT();
  sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();

  if (this->mode_1bit_) {
    slot_config.width = 1;
  } else {
    slot_config.width = 4;
  }

  slot_config.clk = static_cast<gpio_num_t>(this->clk_pin_);
  slot_config.cmd = static_cast<gpio_num_t>(this->cmd_pin_);
  slot_config.d0 = static_cast<gpio_num_t>(this->data0_pin_);
  
  if (!this->mode_1bit_) {
    slot_config.d1 = static_cast<gpio_num_t>(this->data1_pin_);
    slot_config.d2 = static_cast<gpio_num_t>(this->data2_pin_);
    slot_config.d3 = static_cast<gpio_num_t>(this->data3_pin_);
  }

  slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

  auto ret = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot_config, &mount_config, &this->card_);
  if (ret != ESP_OK) {
    if (ret == ESP_FAIL) {
      this->init_error_ = ErrorCode::ERR_MOUNT;
    } else {
      this->init_error_ = ErrorCode::ERR_NO_CARD;
    }
    mark_failed();
    return;
  }

  if (this->sd_card_type_text_sensor_ != nullptr)
    this->sd_card_type_text_sensor_->publish_state(sd_card_type());

  update_sensors();
}

void SdMmc::update_sensors() {
  if (this->card_ == nullptr)
    return;

  FATFS *fs;
  DWORD fre_clust;
  auto res = f_getfree("/sdcard", &fre_clust, &fs);
  if (res != FR_OK) {
    ESP_LOGE(TAG, "Failed to get free space: %d", res);
    return;
  }

  uint64_t total_sectors = (fs->n_fatent - 2) * fs->csize;
  uint64_t free_sectors = fre_clust * fs->csize;
  uint64_t used_sectors = total_sectors - free_sectors;

  uint64_t total_bytes = total_sectors * 512;
  uint64_t free_bytes = free_sectors * 512;
  uint64_t used_bytes = used_sectors * 512;

  if (this->used_space_sensor_ != nullptr)
    this->used_space_sensor_->publish_state(used_bytes);

  if (this->total_space_sensor_ != nullptr)
    this->total_space_sensor_->publish_state(total_bytes);

  if (this->free_space_sensor_ != nullptr)
    this->free_space_sensor_->publish_state(free_bytes);

  for (auto &sensor : this->file_size_sensors_) {
    if (sensor.sensor != nullptr)
      sensor.sensor->publish_state(this->file_size(sensor.path));
  }
}

std::string SdMmc::sd_card_type() const {
  if (this->card_->is_sdio) {
    return "SDIO";
  } else if (this->card_->is_mmc) {
    return "MMC";
  } else {
    return (this->card_->ocr & SD_OCR_SDHC_CAP) ? "SDHC/SDXC" : "SDSC";
  }
}

void SdMmc::write_file(const char *path, const uint8_t *buffer, size_t len, const char *mode) {
  std::string absolut_path = this->build_path(path);
  FILE *file = fopen(absolut_path.c_str(), mode);
  if (!file) {
    ESP_LOGE(TAG, "Failed to open file for writing");
    return;
  }

  size_t written = fwrite(buffer, 1, len, file);
  if (written != len) {
    ESP_LOGE(TAG, "Failed to write to file");
  }

  fclose(file);
  this->update_sensors();
}

bool SdMmc::create_directory(const char *path) {
  std::string absolut_path = this->build_path(path);
  if (mkdir(absolut_path.c_str(), 0777) < 0) {
    ESP_LOGE(TAG, "Failed to create a new directory: %s", strerror(errno));
    return false;
  }
  this->update_sensors();
  return true;
}

bool SdMmc::remove_directory(const char *path) {
  if (!this->is_directory(path)) {
    ESP_LOGE(TAG, "Not a directory");
    return false;
  }

  std::string absolut_path = this->build_path(path);
  if (rmdir(absolut_path.c_str()) < 0) {
    ESP_LOGE(TAG, "Failed to remove directory: %s", strerror(errno));
    return false;
  }
  this->update_sensors();
  return true;
}

bool SdMmc::delete_file(const char *path) {
  if (this->is_directory(path)) {
    ESP_LOGE(TAG, "Not a file");
    return false;
  }

  std::string absolut_path = this->build_path(path);
  if (unlink(absolut_path.c_str()) < 0) {
    ESP_LOGE(TAG, "Failed to remove file: %s", strerror(errno));
    return false;
  }
  this->update_sensors();
  return true;
}

size_t SdMmc::file_size(const char *path) {
  std::string absolut_path = this->build_path(path);
  struct stat info;
  if (stat(absolut_path.c_str(), &info) < 0) {
    ESP_LOGE(TAG, "Failed to stat file: %s", strerror(errno));
    return 0;
  }
  return info.st_size;
}

bool SdMmc::is_directory(const char *path) {
  std::string absolut_path = this->build_path(path);
  DIR *dir = opendir(absolut_path.c_str());
  if (dir) {
    closedir(dir);
    return true;
  }
  return false;
}

std::string SdMmc::build_path(const char *path) {
  std::string result = "/sdcard";
  if (path[0] != '/') {
    result += '/';
  }
  result += path;
  return result;
}

}  // namespace sd_mmc_card
}  // namespace esphome
#endif  // USE_ESP_IDF

