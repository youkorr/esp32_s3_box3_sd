#ifdef USE_ESP32_FRAMEWORK_ARDUINO
#include "sd_mmc_card.h"
#include <SD_MMC.h>

namespace esphome {
namespace sd_mmc_card {

void SdMmc::setup() {
  if(!SD_MMC.begin()) {
    this->init_error_ = ErrorCode::ERR_MOUNT;
    this->mark_failed();
    return;
  }
  
  this->is_setup_ = true;
  update_storage_sensors();
}

void SdMmc::update_storage_sensors() {
  #ifdef USE_SENSOR
  uint64_t total = SD_MMC.totalBytes();
  uint64_t used = SD_MMC.usedBytes();
  
  if(this->total_space_sensor_) {
    this->total_space_sensor_->publish_state(total / (1024.0 * 1024.0));
  }
  if(this->used_space_sensor_) {
    this->used_space_sensor_->publish_state(used / (1024.0 * 1024.0));
  }
  if(this->free_space_sensor_) {
    this->free_space_sensor_->publish_state((total - used) / (1024.0 * 1024.0));
  }
  #endif
}

}  // namespace sd_mmc_card
}  // namespace esphome
#endif
