#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"

#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"
#include "driver/sdspi_host.h"
#include "sdmmc_cmd.h"

#include <vector>
#include <functional>

namespace esphome {
namespace sd_card {

class SDCard : public Component {
 public:
  void setup() override;
  void dump_config() override;
  void loop() override;
  float get_setup_priority() const override { return setup_priority::LATE; }

  void set_clk_pin(uint8_t pin) { this->clk_pin_ = pin; }
  void set_cmd_pin(uint8_t pin) { this->cmd_pin_ = pin; }
  void set_data0_pin(uint8_t pin) { this->data0_pin_ = pin; }
  void set_data1_pin(uint8_t pin) { this->data1_pin_ = pin; }
  void set_data2_pin(uint8_t pin) { this->data2_pin_ = pin; }
  void set_data3_pin(uint8_t pin) { this->data3_pin_ = pin; }
  void set_mode_1bit(bool mode) { this->mode_1bit_ = mode; }
  
  void set_card_type_sensor(text_sensor::TextSensor *card_type) { this->card_type_ = card_type; }
  void set_total_space_sensor(sensor::Sensor *total_space) { this->total_space_ = total_space; }
  void set_used_space_sensor(sensor::Sensor *used_space) { this->used_space_ = used_space; }
  void set_free_space_sensor(sensor::Sensor *free_space) { this->free_space_ = free_space; }

  void update_sensors();

  // Ajout d'un callback pour déclencher des actions lors des mises à jour
  void add_on_update_callback(std::function<void()> callback) {
    this->update_callbacks_.push_back(callback);
  }

 protected:
  uint8_t clk_pin_{0};
  uint8_t cmd_pin_{0};
  uint8_t data0_pin_{0};
  uint8_t data1_pin_{0};
  uint8_t data2_pin_{0};
  uint8_t data3_pin_{0};
  bool mode_1bit_{false};
  bool initialized_{false};
  
  text_sensor::TextSensor *card_type_{nullptr};
  sensor::Sensor *total_space_{nullptr};
  sensor::Sensor *used_space_{nullptr};
  sensor::Sensor *free_space_{nullptr};

  sdmmc_card_t *card_{nullptr};
  FATFS *fs_{nullptr};
  
  uint64_t last_update_{0};

  esp_err_t init_sdmmc();
  esp_err_t mount_fs();
  void unmount_fs();
  std::string get_card_type_str();
  void update_space_info();

  // Liste des callbacks à appeler après une mise à jour
  std::vector<std::function<void()>> update_callbacks_;
};

}  // namespace sd_card
}  // namespace esphome






