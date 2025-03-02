#pragma once
#include "esphome/components/text_sensor/text_sensor.h"


namespace esphome {
namespace sd_box_card {

class SDCardInfoTextSensor : public text_sensor::TextSensor, public Component {
 public:
  void set_parent(SDBoxCard *parent) { parent_ = parent; }
  void set_info_type(const std::string &info_type) { info_type_ = info_type; }
  void setup() override;
  void dump_config() override;

 protected:
  SDBoxCard *parent_;
  std::string info_type_;
};

}  // namespace sd_box_card
}  // namespace esphome
