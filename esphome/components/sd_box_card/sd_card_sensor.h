#pragma once
#include "esphome/components/sensor/sensor.h"
#include "sd_card.h"

namespace esphome {
namespace sd_box_card {

class SDCardDiskUsageSensor : public sensor::Sensor, public Component {
 public:
  void set_parent(SDBoxCard *parent) { parent_ = parent; }
  void set_type(const std::string &type) { type_ = type; }
  void setup() override;
  void dump_config() override;
  void update() override;

 protected:
  SDBoxCard *parent_;
  std::string type_;
};

}  // namespace sd_box_card
}  // namespace esphome
