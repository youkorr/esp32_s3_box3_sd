#pragma once
#include "esphome.h"

namespace esphome {
namespace sd_box_card {

class SDBoxCard : public Component {
 public:
  void set_data3_pin(GPIOPin *pin) { data3_pin_ = pin; }
  void set_cmd_pin(GPIOPin *pin) { cmd_pin_ = pin; }
  void set_data0_pin(GPIOPin *pin) { data0_pin_ = pin; }
  void set_data1_pin(GPIOPin *pin) { data1_pin_ = pin; }
  void set_data2_pin(GPIOPin *pin) { data2_pin_ = pin; }
  void set_clk_pin(GPIOPin *pin) { clk_pin_ = pin; }
  void set_mode_1bit(bool mode) { mode_1bit_ = mode; }

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::LATE; }

 protected:
  GPIOPin *data3_pin_;
  GPIOPin *cmd_pin_;
  GPIOPin *data0_pin_;
  GPIOPin *data1_pin_;
  GPIOPin *data2_pin_;
  GPIOPin *clk_pin_;
  bool mode_1bit_;
};

}  // namespace sd_box_card
}  // namespace esphome









