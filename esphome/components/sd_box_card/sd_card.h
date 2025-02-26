#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "sdmmc_cmd.h"

namespace esphome {
namespace sd_box_card {

class SDBoxCard : public Component {
 public:
  void set_mosi_pin(GPIOPin *pin) { mosi_pin_ = pin; }
  void set_miso_pin(GPIOPin *pin) { miso_pin_ = pin; }
  void set_clk_pin(GPIOPin *pin) { clk_pin_ = pin; }
  void set_cs_pin(GPIOPin *pin) { cs_pin_ = pin; }

  void setup() override;
  void dump_config() override;

 protected:
  GPIOPin *mosi_pin_{nullptr};
  GPIOPin *miso_pin_{nullptr};
  GPIOPin *clk_pin_{nullptr};
  GPIOPin *cs_pin_{nullptr};
  sdmmc_card_t *card_{nullptr};
};

}  // namespace sd_box_card
}  // namespace esphome








