#include "sd_card.h"
#include <driver/sdmmc_host.h>
#include <driver/sdmmc_defs.h>
#include <sdmmc_cmd.h>
#include <esp_vfs_fat.h>

namespace esphome {
namespace sd_box_card {

static const char *TAG = "sd_box_card";

void SDBoxCard::setup() {
  ESP_LOGCONFIG(TAG, "Initializing SD card...");
  
  sdmmc_host_t host = SDMMC_HOST_DEFAULT();
  sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
  
  slot_config.clk = (gpio_num_t) this->clk_pin_->get_pin();
  slot_config.cmd = (gpio_num_t) this->cmd_pin_->get_pin();
  slot_config.d0 = (gpio_num_t) this->data0_pin_->get_pin();
  
  if(!this->mode_1bit_) {
    slot_config.d1 = (gpio_num_t) this->data1_pin_->get_pin();
    slot_config.d2 = (gpio_num_t) this->data2_pin_->get_pin();
    slot_config.d3 = (gpio_num_t) this->data3_pin_->get_pin();
    slot_config.width = 4;
  } else {
    slot_config.width = 1;
  }

  esp_vfs_fat_sdmmc_mount_config_t mount_config = {
      .format_if_mount_failed = false,
      .max_files = 5,
      .allocation_unit_size = 16 * 1024
  };

  sdmmc_card_t *card;
  esp_err_t ret = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot_config, &mount_config, &card);

  if(ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to initialize SD card: %s", esp_err_to_name(ret));
    this->mark_failed();
    return;
  }
  
  ESP_LOGI(TAG, "SD card initialized successfully");
}

void SDBoxCard::dump_config() {
  ESP_LOGCONFIG(TAG, "SD Card Configuration:");
  LOG_PIN("  CLK Pin: ", this->clk_pin_);
  LOG_PIN("  CMD Pin: ", this->cmd_pin_);
  LOG_PIN("  DATA0 Pin: ", this->data0_pin_);
  if(!this->mode_1bit_) {
    LOG_PIN("  DATA1 Pin: ", this->data1_pin_);
    LOG_PIN("  DATA2 Pin: ", this->data2_pin_);
    LOG_PIN("  DATA3 Pin: ", this->data3_pin_);
  }
}

}  // namespace sd_box_card
}  // namespace esphome










