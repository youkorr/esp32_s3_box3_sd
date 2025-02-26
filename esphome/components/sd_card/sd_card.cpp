#include "sd_card.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include <sys/stat.h>

static const char *TAG = "sd_card";

#define SECTOR_SIZE 512  // Taille standard du secteur

namespace esphome {
namespace sd_card {

void SDCard::setup() {
  ESP_LOGCONFIG(TAG, "Setting up SD Card...");

  // Initialiser le SDMMC
  esp_err_t ret = this->init_sdmmc();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to initialize SD card");
    this->mark_failed();
    return;
  }

  // Monter le système de fichiers
  ret = this->mount_fs();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to mount filesystem");
    this->mark_failed();
    return;
  }

  this->initialized_ = true;

  if (this->card_type_ != nullptr) {
    this->card_type_->publish_state(this->get_card_type_str());
  }

  this->update_sensors();
}

void SDCard::update_sensors() {
  if (!this->initialized_)
    return;

  if (this->card_type_ != nullptr) {
    this->card_type_->publish_state(this->get_card_type_str());
  }

  this->update_space_info();

  // Appeler les callbacks d'événements
  for (const auto &callback : this->update_callbacks_) {
    callback();
  }
}

esp_err_t SDCard::init_sdmmc() {
  ESP_LOGD(TAG, "Initializing SDMMC");

  sdmmc_host_t host = SDMMC_HOST_DEFAULT();
  sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();

  slot_config.clk = static_cast<gpio_num_t>(this->clk_pin_);
  slot_config.cmd = static_cast<gpio_num_t>(this->cmd_pin_);
  slot_config.d0 = static_cast<gpio_num_t>(this->data0_pin_);

  if (!this->mode_1bit_) {
    slot_config.d1 = static_cast<gpio_num_t>(this->data1_pin_);
    slot_config.d2 = static_cast<gpio_num_t>(this->data2_pin_);
    slot_config.d3 = static_cast<gpio_num_t>(this->data3_pin_);
    slot_config.width = 4;
  } else {
    slot_config.width = 1;
  }

  esp_vfs_fat_sdmmc_mount_config_t mount_config = {
      .format_if_mount_failed = false,
      .max_files = 5,
      .allocation_unit_size = 16 * 1024
  };

  ESP_LOGD(TAG, "Mounting filesystem");
  esp_err_t ret = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot_config, &mount_config, &this->card_);

  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "SD card initialization failed");
    return ret;
  }

  return ESP_OK;
}

std::string SDCard::get_card_type_str() {
  if (this->card_ == nullptr)
    return "Unknown";

  uint64_t capacity = (uint64_t)this->card_->csd.capacity * this->card_->csd.sector_size;

  if (capacity <= 2ULL * 1024 * 1024 * 1024) {  // ≤ 2GB → SDSC
    return "SDSC";
  } else if (capacity <= 32ULL * 1024 * 1024 * 1024) {  // ≤ 32GB → SDHC
    return "SDHC";
  } else {  // > 32GB → SDXC
    return "SDXC";
  }
}


void SDCard::update_space_info() {
  if (!this->initialized_)
    return;

  FATFS *fs;
  DWORD fre_clust;

  FRESULT res = f_getfree("/sdcard", &fre_clust, &fs);
  if (res != FR_OK) {
    ESP_LOGE(TAG, "Failed to get free space");
    return;
  }

  uint64_t total_bytes = ((uint64_t) fs->csize) * SECTOR_SIZE * (fs->n_fatent - 2);
  uint64_t free_bytes = ((uint64_t) fs->csize) * SECTOR_SIZE * fre_clust;
  uint64_t used_bytes = total_bytes - free_bytes;

  float total_gb = total_bytes / (1024.0 * 1024.0 * 1024.0);
  float free_gb = free_bytes / (1024.0 * 1024.0 * 1024.0);
  float used_gb = used_bytes / (1024.0 * 1024.0 * 1024.0);

  if (this->total_space_ != nullptr) {
    this->total_space_->publish_state(total_gb);
  }

  if (this->free_space_ != nullptr) {
    this->free_space_->publish_state(free_gb);
  }

  if (this->used_space_ != nullptr) {
    this->used_space_->publish_state(used_gb);
  }
}

}  // namespace sd_card
}  // namespace esphome











