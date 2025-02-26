#include "sd_card_text_sensor.h"

namespace esphome {
namespace sd_box_card {

static const char *TAG = "sd_card.text_sensor";

void SDCardInfoTextSensor::setup() {
  if(!this->parent_->is_failed()) {
    const char* info = "";
    if(this->info_type_ == "card_type") {
      info = this->parent_->get_card_type();
    }
    this->publish_state(info);
  }
}

void SDCardInfoTextSensor::dump_config() {
  ESP_LOGCONFIG(TAG, "SD Card Info Sensor:");
  ESP_LOGCONFIG(TAG, "  Info Type: %s", this->info_type_.c_str());
}
