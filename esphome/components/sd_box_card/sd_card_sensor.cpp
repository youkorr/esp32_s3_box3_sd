#include "sd_card_sensor.h"

namespace esphome {


static const char *TAG = "sd_card.sensor";

void SDCardDiskUsageSensor::setup() {
  if(!this->parent_->is_failed()) {
    this->publish_state(0);
  }
}

void SDCardDiskUsageSensor::update() {
  if(this->parent_->is_failed()) return;

  float value = 0;
  if(this->type_ == "free") {
    value = this->parent_->get_free_space();
  } else if(this->type_ == "used") {
    value = this->parent_->get_used_space();
  } else if(this->type_ == "total") {
    value = this->parent_->get_total_space();
  }
  
  this->publish_state(value);
}

void SDCardDiskUsageSensor::dump_config() {
  ESP_LOGCONFIG(TAG, "SD Card Usage Sensor:");
  ESP_LOGCONFIG(TAG, "  Type: %s", this->type_.c_str());
}
