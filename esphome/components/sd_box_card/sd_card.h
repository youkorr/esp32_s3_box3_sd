#pragma once
#include "esphome.h"
#include <driver/sdmmc_host.h>
#include <driver/sdmmc_defs.h>
#include <sdmmc_cmd.h>
#include <esp_vfs_fat.h>

namespace esphome {
namespace sd_box_card {

class SDBoxCard : public Component {
 public:
  // Configuration pins
  void set_data3_pin(GPIOPin *pin) { data3_pin_ = pin; }
  void set_cmd_pin(GPIOPin *pin) { cmd_pin_ = pin; }
  void set_data0_pin(GPIOPin *pin) { data0_pin_ = pin; }
  void set_data1_pin(GPIOPin *pin) { data1_pin_ = pin; }
  void set_data2_pin(GPIOPin *pin) { data2_pin_ = pin; }
  void set_clk_pin(GPIOPin *pin) { clk_pin_ = pin; }
  void set_mode_1bit(bool mode) { mode_1bit_ = mode; }

  // Core functions
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::LATE; }

  // Sensor accessors
  float get_free_space();
  float get_used_space();
  float get_total_space();
  const char* get_card_type();
  sdmmc_card_t* get_card() { return card_; }

 protected:
  GPIOPin *data3_pin_{nullptr};
  GPIOPin *cmd_pin_{nullptr};
  GPIOPin *data0_pin_{nullptr};
  GPIOPin *data1_pin_{nullptr};
  GPIOPin *data2_pin_{nullptr};
  GPIOPin *clk_pin_{nullptr};
  bool mode_1bit_{false};
  sdmmc_card_t *card_{nullptr};
  uint64_t total_bytes_{0};
};

}  // namespace sd_box_card
}  // namespace esphome









