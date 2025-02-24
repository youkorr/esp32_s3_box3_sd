#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include <SD.h>

namespace esphome {
namespace esp32_s3_box3_sd {

class ESP32S3Box3SDCard : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  
  void set_space_used_sensor(sensor::Sensor *space_used_sensor) { space_used_sensor_ = space_used_sensor; }
  void set_total_space_sensor(sensor::Sensor *total_space_sensor) { total_space_sensor_ = total_space_sensor; }

 protected:
  void update_sensors_();

  sensor::Sensor *space_used_sensor_{nullptr};
  sensor::Sensor *total_space_sensor_{nullptr};

  static const int SD_CMD = 38;
  static const int SD_CLK = 39;
  static const int SD_D0 = 40;
  static const int SD_D1 = 41;
  static const int SD_D2 = 42;
  static const int SD_D3 = 45;
  static const int SD_DET = 21;
};

}  // namespace esp32_s3_box3_sd
}  // namespace esphome
