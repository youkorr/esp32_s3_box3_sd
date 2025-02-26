#include "sd_card.h"
#include "esp_vfs_fat.h"
#include "ff.h"
#include <cinttypes>

namespace esphome {
namespace sd_box_card {

static const char *TAG = "sd_box_card";

void SDBoxCard::setup() {
  ESP_LOGCONFIG(TAG, "Initializing SD card...");

  sdmmc_host_t host = SDMMC_HOST_DEFAULT();
  sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();

  // Configure pins
  slot_config.clk = static_cast<gpio_num_t>(clk_pin_->get_pin());
  slot_config.cmd = static_cast<gpio_num_t>(cmd_pin_->get_pin());
  slot_config.d0 = static_cast<gpio_num_t>(data0_pin_->get_pin());
  
  if(!mode_1bit_) {
    slot_config.d1 = static_cast<gpio_num_t>(data1_pin_->get_pin());
    slot_config.d2 = static_cast<gpio_num_t>(data2_pin_->get_pin());
    slot_config.d3 = static_cast<gpio_num_t>(data3_pin_->get_pin());
    slot_config.width = 4;
  } else {
    slot_config.width = 1;
  }

  // Mount configuration
  esp_vfs_fat_sdmmc_mount_config_t mount_config = {
      .format_if_mount_failed = false,
      .max_files = 5,
      .allocation_unit_size = 16 * 1024
  };

  // Mount SD card
  esp_err_t ret = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot_config, &mount_config, &card_);
  
  if(ret != ESP_OK) {
    ESP_LOGE(TAG, "Mount failed: %s (0x%x)", esp_err_to_name(ret), ret);
    this->mark_failed();
    return;
  }

  // Get card information
  total_bytes_ = static_cast<uint64_t>(card_->csd.capacity) * card_->csd.sector_size;
  ESP_LOGI(TAG, 
    "SD card mounted\n"
    "Name: %s\n"
    "Type: %s\n"
    "Speed: %s\n"
    "Size: %.2f MB",
    card_->cid.name,
    (card_->ocr & SD_OCR_CARD_CAPACITY) ? "SDHC/SDXC" : "SDSC",
    (card_->max_freq_khz < 26000) ? "Default Speed" : "High Speed",
    total_bytes_ / (1024.0f * 1024.0f)
  );
}

void SDBoxCard::dump_config() {
  ESP_LOGCONFIG(TAG, "SD Card Configuration:");
  LOG_PIN("  CLK Pin: ", clk_pin_);
  LOG_PIN("  CMD Pin: ", cmd_pin_);
  LOG_PIN("  DATA0 Pin: ", data0_pin_);
  if(!mode_1bit_) {
    LOG_PIN("  DATA1 Pin: ", data1_pin_);
    LOG_PIN("  DATA2 Pin: ", data2_pin_);
    LOG_PIN("  DATA3 Pin: ", data3_pin_);
  }
}

float SDBoxCard::get_free_space() {
  if(!card_ || this->is_failed()) return 0.0f;
  
  FATFS* fs;
  DWORD fre_clust;
  FRESULT res = f_getfree("", &fre_clust, &fs);
  
  if(res != FR_OK) {
    ESP_LOGE(TAG, "Free space check failed: %d", res);
    return 0.0f;
  }
  
  return (fs->csize * fre_clust * card_->csd.sector_size) / (1024.0f * 1024.0f);
}

float SDBoxCard::get_used_space() {
  return (total_bytes_ / (1024.0f * 1024.0f)) - get_free_space();
}

float SDBoxCard::get_total_space() {
  return total_bytes_ / (1024.0f * 1024.0f);
}

const char* SDBoxCard::get_card_type() {
  if(!card_ || this->is_failed()) return "UNKNOWN";
  
  if(card_->ocr & SD_OCR_CARD_CAPACITY) {
    return (card_->csd.capacity > 70000000) ? "SDXC" : "SDHC";
  }
  return "SDSC";
}

}  // namespace sd_box_card
}  // namespace esphome










