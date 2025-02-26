#include "sd_card.h"
#include "esphome/core/log.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "sdmmc_cmd.h"

namespace esphome {
namespace sd_box_card {

static const char *TAG = "sd_box_card";

void SDBoxCard::setup() {
  ESP_LOGCONFIG(TAG, "Setting up SD Card (SPI mode)...");

  spi_bus_config_t bus_cfg = {
    .mosi_io_num = cmd_pin_->get_pin(),
    .miso_io_num = data0_pin_->get_pin(),
    .sclk_io_num = clk_pin_->get_pin(),
    .quadwp_io_num = -1,
    .quadhd_io_num = -1,
    .max_transfer_sz = 4000,
  };

  esp_err_t ret = spi_bus_initialize(VSPI_HOST, &bus_cfg, 1);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to initialize SPI bus");
    return;
  }

  sdmmc_host_t host = SDSPI_HOST_DEFAULT();
  sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
  slot_config.gpio_cs = static_cast<gpio_num_t>(data3_pin_->get_pin());
  slot_config.host_id = VSPI_HOST;

  esp_vfs_fat_sdmmc_mount_config_t mount_config = {
    .format_if_mount_failed = false,
    .max_files = 5,
    .allocation_unit_size = 16 * 1024
  };

  ret = esp_vfs_fat_sdspi_mount("/sdcard", &host, &slot_config, &mount_config, &card_);

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
  ESP_LOGCONFIG(TAG, "SD Card (SPI mode):");
  LOG_PIN("  MOSI Pin: ", this->cmd_pin_);
  LOG_PIN("  MISO Pin: ", this->data0_pin_);
  LOG_PIN("  CLK Pin: ", this->clk_pin_);
  LOG_PIN("  CS Pin: ", this->data3_pin_);
}

}  // namespace sd_box_card
}  // namespace esphome








