#pragma once
#include "esphome/core/gpio.h"
#include "esphome/core/defines.h"
#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include <map>
#include <string>
#include <vector>

namespace esphome {
namespace sensor { class Sensor; }
namespace text_sensor { class TextSensor; }

namespace sd_mmc_card {

enum class MemoryUnits { BYTES, KILOBYTES, MEGABYTES, GIGABYTES };

struct FileInfo {
  std::string path;
  size_t size;
  bool is_directory;
  FileInfo(std::string const &path, size_t size, bool is_directory);
};

class SdMmc : public Component {
 public:
  enum class ErrorCode { NONE, ERR_PIN_SETUP, ERR_MOUNT, ERR_NO_CARD };

  void set_clk_pin(uint8_t pin) { clk_pin_ = pin; }
  void set_cmd_pin(uint8_t pin) { cmd_pin_ = pin; }
  void set_d0_pin(uint8_t pin) { d0_pin_ = pin; }
  void set_d1_pin(uint8_t pin) { d1_pin_ = pin; }
  void set_d2_pin(uint8_t pin) { d2_pin_ = pin; }
  void set_d3_pin(uint8_t pin) { d3_pin_ = pin; }
  void set_mode_1bit(bool mode) { mode_1bit_ = mode; }

  // File operations
  bool delete_file(const char *path);
  size_t file_size(const char *path);
  bool is_directory(const char *path);
  std::vector<uint8_t> read_file(const char *path);
  std::vector<std::string> list_directory(const char *path, uint8_t depth = 0);
  std::vector<FileInfo> list_directory_file_info(const char *path, uint8_t depth = 0);

  // Sensor integration
  #ifdef USE_SENSOR
  class FileSizeSensor {
   public:
    FileSizeSensor(esphome::sensor::Sensor *sensor, std::string const &path);
    esphome::sensor::Sensor *sensor;
    std::string path;
  };
  void add_file_size_sensor(esphome::sensor::Sensor *sensor, std::string const &path);
  #endif

  // Memory conversion
  static long double convertBytes(uint64_t value, MemoryUnits unit);

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  #ifdef USE_TEXT_SENSOR
  void set_sd_card_type_text_sensor(esphome::text_sensor::TextSensor *sensor) { 
    sd_card_type_text_sensor_ = sensor; 
  }
  #endif

 protected:
  // Hardware pins
  uint8_t clk_pin_;
  uint8_t cmd_pin_;
  uint8_t d0_pin_;
  uint8_t d1_pin_;
  uint8_t d2_pin_;
  uint8_t d3_pin_;
  bool mode_1bit_{false};
  
  // Sensor pointers
  #ifdef USE_SENSOR
  esphome::sensor::Sensor *used_space_sensor_{nullptr};
  esphome::sensor::Sensor *total_space_sensor_{nullptr};
  esphome::sensor::Sensor *free_space_sensor_{nullptr};
  std::vector<FileSizeSensor> file_size_sensors_;
  #endif
  
  #ifdef USE_TEXT_SENSOR
  esphome::text_sensor::TextSensor *sd_card_type_text_sensor_{nullptr};
  #endif

  virtual void update_storage_sensors();
};

}  // namespace sd_mmc_card
}  // namespace esphome
