#pragma once
#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include <string>
#include <vector>
#include <driver/sdmmc_host.h>
#include <driver/sdspi_host.h>
#include <esp_vfs_fat.h>

namespace esphome {
namespace sd_card {

class SdMmc : public Component {
public:
  void setup() override;
  void loop() override;
  void dump_config() override;

  // Configuration des broches
  void set_cs_pin(uint8_t pin) { cs_pin_ = pin; }
  void set_mosi_pin(uint8_t pin) { mosi_pin_ = pin; }
  void set_miso_pin(uint8_t pin) { miso_pin_ = pin; }
  void set_clk_pin(uint8_t pin) { clk_pin_ = pin; }

  // Gestion fichiers
  bool file_exists(const char *path);
  std::vector<uint8_t> read_file(const char *path);

private:
  uint8_t cs_pin_;
  uint8_t mosi_pin_;
  uint8_t miso_pin_;
  uint8_t clk_pin_;
  bool mounted_ = false;
  sdmmc_card_t *card_ = nullptr;
};

} // namespace sd_card
} // namespace esphome


