#include "sd_card.h"
#include "esphome/core/log.h"
#include <driver/spi_master.h>
#include <sdmmc_cmd.h>

namespace esphome {
namespace sd_card {

static const char *TAG = "sd_card";

void SdMmc::setup() {
  esp_err_t ret;
  
  // Configuration SPI avec conversion explicite
  spi_bus_config_t bus_cfg = {
    .mosi_io_num = mosi_pin_,
    .miso_io_num = miso_pin_,
    .sclk_io_num = clk_pin_,
    .quadwp_io_num = GPIO_NUM_NC,
    .quadhd_io_num = GPIO_NUM_NC,
    .max_transfer_sz = 4092
  };

  sdmmc_host_t host = SDSPI_HOST_DEFAULT();
  sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
  
  // Assignation directe avec les bons types
  slot_config.gpio_cs = cs_pin_;
  slot_config.host_id = SPI2_HOST;

  // Montage avec gestion d'erreur complète
  esp_vfs_fat_sdmmc_mount_config_t mount_config = {
    .format_if_mount_failed = false,
    .max_files = 5,
    .allocation_unit_size = 16 * 1024
  };

  if ((ret = spi_bus_initialize(SPI2_HOST, &bus_cfg, SDSPI_DEFAULT_DMA)) != ESP_OK) {
    ESP_LOGE(TAG, "SPI init failed: 0x%X", ret);
    return;
  }

  if ((ret = esp_vfs_fat_sdspi_mount("/sdcard", &host, &slot_config, &mount_config, &card_)) != ESP_OK) {
    ESP_LOGE(TAG, "Mount failed: 0x%X", ret);
    spi_bus_free(SPI2_HOST);
    return;
  }

  mounted_ = true;
  ESP_LOGI(TAG, "SD Card mounted successfully");
}

bool SdMmc::file_exists(const char *path) {
  if (!mounted_) return false;
  
  FILE *f = fopen(path, "r");
  if (f == nullptr) return false;
  fclose(f);
  return true;
}

std::vector<uint8_t> SdMmc::read_file(const char *path) {
  std::vector<uint8_t> data;
  
  if (!mounted_) return data;
  
  FILE *f = fopen(path, "rb");
  if (!f) {
    ESP_LOGE(TAG, "Failed to open file: %s", path);
    return data;
  }

  fseek(f, 0, SEEK_END);
  size_t size = ftell(f);
  rewind(f);

  data.resize(size);
  fread(data.data(), 1, size, f);
  fclose(f);
  
  return data;
}

} // namespace sd_card
} // namespace esphome

