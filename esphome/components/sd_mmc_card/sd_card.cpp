#include "sd_card.h"
#include "esphome/core/log.h"

namespace esphome {
namespace sd_box_card {

static const char *TAG = "sd_box_card";

void SDBoxCard::setup() {
  ESP_LOGCONFIG(TAG, "Setting up SD Card...");

  sdmmc_host_t host = SDMMC_HOST_DEFAULT();
  sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();

  slot_config.width = mode_1bit_ ? 1 : 4;
  slot_config.clk = clk_pin_->get_pin();
  slot_config.cmd = cmd_pin_->get_pin();
  slot_config.d0 = data0_pin_->get_pin();
  if (!mode_1bit_) {
    slot_config.d1 = data1_pin_->get_pin();
    slot_config.d2 = data2_pin_->get_pin();
    slot_config.d3 = data3_pin_->get_pin();
  }

  esp_vfs_fat_sdmmc_mount_config_t mount_config = {
      .format_if_mount_failed = false,
      .max_files = 5,
      .allocation_unit_size = 16 * 1024
  };

  esp_err_t ret = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot_config, &mount_config, &card_);

  if (ret != ESP_OK) {
    if (ret == ESP_FAIL) {
      ESP_LOGE(TAG, "Failed to mount filesystem.");
    } else {
      ESP_LOGE(TAG, "Failed to initialize the card (%s). "
               "Make sure SD card lines have pull-up resistors in place.",
               esp_err_to_name(ret));
    }
    return;
  }

  ESP_LOGI(TAG, "SD Card mounted successfully");
}

void SDBoxCard::dump_config() {
  ESP_LOGCONFIG(TAG, "SD Box Card:");
  LOG_PIN("  CLK Pin: ", this->clk_pin_);
  LOG_PIN("  CMD Pin: ", this->cmd_pin_);
  LOG_PIN("  D0 Pin: ", this->data0_pin_);
  if (!this->mode_1bit_) {
    LOG_PIN("  D1 Pin: ", this->data1_pin_);
    LOG_PIN("  D2 Pin: ", this->data2_pin_);
    LOG_PIN("  D3 Pin: ", this->data3_pin_);
  }
  ESP_LOGCONFIG(TAG, "  Mode: %s", this->mode_1bit_ ? "1-bit" : "4-bit");
}

}  // namespace sd_box_card
}  // namespace esphome






