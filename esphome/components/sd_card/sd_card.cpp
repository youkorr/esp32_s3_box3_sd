#include "sd_card.h"
#include "esphome/core/log.h"
#include <driver/sdmmc_host.h>
#include <driver/sdspi_host.h>
#include <sdmmc_cmd.h>
#include <esp_vfs_fat.h>

namespace esphome {
namespace sd_card {

static const char *TAG = "sd_card";

void SdMmc::setup() {
  sdmmc_host_t host = SDSPI_HOST_DEFAULT();
  sdspi_slot_config_t slot_config = SDSPI_SLOT_CONFIG_DEFAULT();
  slot_config.gpio_miso = miso_pin_;
  slot_config.gpio_mosi = mosi_pin_;
  slot_config.gpio_sck = clk_pin_;
  slot_config.gpio_cs = cs_pin_;

  esp_vfs_fat_sdmmc_mount_config_t mount_config = {
      .format_if_mount_failed = false,
      .max_files = 5,
      .allocation_unit_size = 16 * 1024};

  sdmmc_card_t *card;
  esp_err_t ret = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot_config, &mount_config, &card);

  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to mount SD card: %s", esp_err_to_name(ret));
    this->mark_failed();
    mounted_ = false;
    return;
  }
  ESP_LOGI(TAG, "SD card mounted successfully");
  mounted_ = true;
}

void SdMmc::loop() {}

bool SdMmc::play_audio(const char *path) {
  if (!mounted_) {
    ESP_LOGE(TAG, "SD card is not mounted");
    return false;
  }
  // Implémente la lecture audio ici
  return true;
}

bool SdMmc::load_image(const char *path, uint8_t *buffer, size_t buffer_size) {
  if (!mounted_) {
    ESP_LOGE(TAG, "SD card is not mounted");
    return false;
  }
  // Implémente le chargement d'image ici
  return true;
}

}  // namespace sd_card
}  // namespace esphome

