#ifdef USE_ESP_IDF
#include "sd_mmc_card.h"
#include <dirent.h>
#include <sys/stat.h>
#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"
#include "driver/sdmmc_defs.h"
#include "esp_err.h"

namespace esphome {
namespace sd_mmc_card {

void SdMmc::setup() {
  sdmmc_host_t host = SDMMC_HOST_DEFAULT();
  sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
  
  slot_config.clk = (gpio_num_t)this->clk_pin_;
  slot_config.cmd = (gpio_num_t)this->cmd_pin_;
  slot_config.width = this->mode_1bit_ ? 1 : 4;
  
  if(!this->mode_1bit_) {
    slot_config.d0 = (gpio_num_t)this->d0_pin_;
    slot_config.d1 = (gpio_num_t)this->d1_pin_;
    slot_config.d2 = (gpio_num_t)this->d2_pin_;
    slot_config.d3 = (gpio_num_t)this->d3_pin_;
  }

  esp_vfs_fat_sdmmc_mount_config_t mount_config = {
    .format_if_mount_failed = false,
    .max_files = 5,
    .allocation_unit_size = 16 * 1024
  };

  sdmmc_card_t *card;
  esp_err_t ret = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot_config, 
                                        &mount_config, &card);

  if(ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to mount SD/MMC card (0x%x)", ret);
    this->init_error_ = ErrorCode::ERR_MOUNT;
    this->mark_failed();
    return;
  }

  this->is_setup_ = true;
  update_storage_sensors();
}

void SdMmc::update_storage_sensors() {
  FATFS *fs;
  DWORD free_clusters;
  FATFS *get_fatfs_ret = get_fatfs(&fs);
  
  if(get_fatfs_ret == NULL) return;

  if(f_getfree("", &free_clusters, &fs) != FR_OK) return;

  uint64_t total = ((uint64_t)(fs->n_fatent - 2)) * fs->csize * 512;
  uint64_t free = (uint64_t)free_clusters * fs->csize * 512;
  
  #ifdef USE_SENSOR
  if(this->total_space_sensor_) {
    this->total_space_sensor_->publish_state(total / (1024.0 * 1024.0));
  }
  if(this->free_space_sensor_) {
    this->free_space_sensor_->publish_state(free / (1024.0 * 1024.0));
  }
  if(this->used_space_sensor_) {
    this->used_space_sensor_->publish_state((total - free) / (1024.0 * 1024.0));
  }
  #endif
}

}  // namespace sd_mmc_card
}  // namespace esphome
#endif

