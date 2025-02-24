#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include <FS.h>
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
  bool sdcard_is_mounted(); // Add this line

protected:
  void update_sensors_();

  sensor::Sensor *space_used_sensor_{nullptr};
  sensor::Sensor *total_space_sensor_{nullptr};

  static const int SD_CS = 12;   // Chip Select
  static const int SD_MOSI = 14; // MOSI
  static const int SD_MISO = 19; // MISO
  static const int SD_SCK = 11;   // Clock
};

}  // namespace esp32_s3_box3_sd
}  // namespace esphome




