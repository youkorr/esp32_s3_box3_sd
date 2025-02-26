#include "sd_card.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include <sys/stat.h>

static const char *TAG = "sd_card";

// Définir la taille du secteur
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
  
  this->update_space_info();
}

void SDCard::dump_config() {
  ESP_LOGCONFIG(TAG, "SD Card:");
  ESP_LOGCONFIG(TAG, "  CLK Pin: %d", this->clk_pin_);
  ESP_LOGCONFIG(TAG, "  CMD Pin: %d", this->cmd_pin_);
  ESP_LOGCONFIG(TAG, "  DATA0 Pin: %d", this->data0_pin_);
  
  if (!this->mode_1bit_) {
    ESP_LOGCONFIG(TAG, "  DATA1 Pin: %d", this->data1_pin_);
    ESP_LOGCONFIG(TAG, "  DATA2 Pin: %d", this->data2_pin_);
    ESP_LOGCONFIG(TAG, "  DATA3 Pin: %d", this->data3_pin_);
  }
  
  ESP_LOGCONFIG(TAG, "  Mode: %s", this->mode_1bit_ ? "1-bit" : "4-bit");
  
  if (this->initialized_) {
    ESP_LOGCONFIG(TAG, "  Card Type: %s", this->get_card_type_str().c_str());
    ESP_LOGCONFIG(TAG, "  Mounted at: /sdcard");
    
    FATFS *fs;
    DWORD fre_clust;
    uint64_t total_bytes, free_bytes;
    
    f_getfree("/sdcard", &fre_clust, &fs);
    
    total_bytes = ((uint64_t) fs->csize) * SECTOR_SIZE * ((fs->n_fatent - 2));
    free_bytes = ((uint64_t) fs->csize) * SECTOR_SIZE * fre_clust;
    uint64_t used_bytes = total_bytes - free_bytes;
    
    ESP_LOGCONFIG(TAG, "  Total space: %.2f GB", total_bytes / (1024.0 * 1024.0 * 1024.0));
    ESP_LOGCONFIG(TAG, "  Free space: %.2f GB", free_bytes / (1024.0 * 1024.0 * 1024.0));
    ESP_LOGCONFIG(TAG, "  Used space: %.2f GB", used_bytes / (1024.0 * 1024.0 * 1024.0));
  } else {
    ESP_LOGCONFIG(TAG, "  Initialization failed");
  }
}

void SDCard::loop() {
  if (!this->initialized_)
    return;

  const uint32_t now = millis();
  if (now - this->last_update_ < 60000)  // Update every minute
    return;
    
  this->last_update_ = now;
  this->update_sensors();
}

void SDCard::update_sensors() {
  if (this->card_type_ != nullptr) {
    this->card_type_->publish_state(this->get_card_type_str());
  }
  
  this->update_space_info();
}

esp_err_t SDCard::init_sdmmc() {
  ESP_LOGD(TAG, "Initializing SDMMC");
  
  sdmmc_host_t host = SDMMC_HOST_DEFAULT();
  
  // Configurer les broches pour SDMMC
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
  
  // Options pour l'initialisation de la carte SD
  esp_vfs_fat_sdmmc_mount_config_t mount_config = {
      .format_if_mount_failed = false,
      .max_files = 5,
      .allocation_unit_size = 16 * 1024
  };
  
  ESP_LOGD(TAG, "Mounting filesystem");
  esp_err_t ret = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot_config, &mount_config, &this->card_);
  
  if (ret != ESP_OK) {
    if (ret == ESP_FAIL) {
      ESP_LOGE(TAG, "Failed to mount filesystem");
    } else {
      ESP_LOGE(TAG, "Failed to initialize SD card");
    }
    return ret;
  }
  
  return ESP_OK;
}

esp_err_t SDCard::mount_fs() {
  if (this->card_ == nullptr) {
    return ESP_FAIL;
  }
  
  ESP_LOGD(TAG, "SD card mounted successfully");
  return ESP_OK;
}

void SDCard::unmount_fs() {
  if (this->card_ != nullptr) {
    esp_vfs_fat_sdcard_unmount("/sdcard", this->card_);
    this->card_ = nullptr;
    ESP_LOGD(TAG, "Card unmounted");
  }
}

std::string SDCard::get_card_type_str() {
  if (this->card_ == nullptr)
    return "Unknown";
    
  sdmmc_card_t *card = this->card_;
  uint32_t type = sdmmc_card_get_type(card);  // Obtenir le type de la carte

  switch (type) {
    case SDMMC_CARD_TYPE_SDSC:
      return "SDSC";
    case SDMMC_CARD_TYPE_SDHC:
      return "SDHC";
    case SDMMC_CARD_TYPE_SDXC:
      return "SDXC";
    case SDMMC_CARD_TYPE_MMC:
      return "MMC";
    case SDMMC_CARD_TYPE_MMC_HC:
      return "MMC HC";
    default:
      return "Unknown";
  }
}

void SDCard::update_space_info() {
  if (!this->initialized_ || (this->total_space_ == nullptr && this->free_space_ == nullptr && this->used_space_ == nullptr))
    return;
    
  FATFS *fs;
  DWORD fre_clust;
  
  FRESULT res = f_getfree("/sdcard", &fre_clust, &fs);
  if (res != FR_OK) {
    ESP_LOGE(TAG, "Failed to get free space");
    return;
  }
  
  uint64_t total_bytes = ((uint64_t) fs->csize) * SECTOR_SIZE * ((fs->n_fatent - 2));
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







