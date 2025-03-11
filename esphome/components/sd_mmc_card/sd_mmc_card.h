#pragma once
#include "esphome/core/gpio.h"
#include "esphome/core/defines.h"
#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif

#ifdef USE_ESP_IDF
#include "sdmmc_cmd.h"
#endif

namespace esphome {
namespace sd_mmc_card {

enum MemoryUnits : short { Byte = 0, KiloByte = 1, MegaByte = 2, GigaByte = 3, TeraByte = 4, PetaByte = 5 };

#ifdef USE_SENSOR
struct FileSizeSensor {
  sensor::Sensor *sensor{nullptr};
  std::string path;

  FileSizeSensor() = default;
  FileSizeSensor(sensor::Sensor *sensor, std::string const &path) : sensor(sensor), path(path) {}
};
#endif

struct FileInfo {
  std::string path;
  size_t size;
  bool is_directory;

  FileInfo(std::string const &path, size_t size, bool is_directory)
      : path(path), size(size), is_directory(is_directory) {}
};

class SdMmc : public Component {
#ifdef USE_SENSOR
  SUB_SENSOR(used_space)
  SUB_SENSOR(total_space)
  SUB_SENSOR(free_space)
#endif
#ifdef USE_TEXT_SENSOR
  SUB_TEXT_SENSOR(sd_card_type)
#endif
 public:
  enum ErrorCode {
    ERR_PIN_SETUP,
    ERR_MOUNT,
    ERR_NO_CARD,
    NO_ERROR
  };
  
  void setup() override;
  void loop() override;
  void dump_config() override;
  
  void write_file(const char *path, const uint8_t *buffer, size_t len, const char *mode);
  void write_file(const char *path, const uint8_t *buffer, size_t len) {
    this->write_file(path, buffer, len, "wb");
  }
  
  void append_file(const char *path, const uint8_t *buffer, size_t len) {
    this->write_file(path, buffer, len, "ab");
  }
  
  bool delete_file(const char *path);
  bool delete_file(std::string const &path) {
    return this->delete_file(path.c_str());
  }
  
  bool create_directory(const char *path);
  bool remove_directory(const char *path);
  
  std::vector<uint8_t> read_file(char const *path);
  std::vector<uint8_t> read_file(std::string const &path) {
    return this->read_file(path.c_str());
  }
  
  bool read_file_chunk(const char *path, uint8_t *buffer, size_t offset, size_t len);
  bool read_file_chunk(std::string const &path, uint8_t *buffer, size_t offset, size_t len) {
    return this->read_file_chunk(path.c_str(), buffer, offset, len);
  }
  
  bool is_directory(const char *path);
  bool is_directory(std::string const &path) {
    return this->is_directory(path.c_str());
  }
  
  std::vector<std::string> list_directory(const char *path, uint8_t depth);
  std::vector<std::string> list_directory(std::string path, uint8_t depth) {
    return this->list_directory(path.c_str(), depth);
  }
  
  std::vector<FileInfo> list_directory_file_info(const char *path, uint8_t depth);
  std::vector<FileInfo> list_directory_file_info(std::string path, uint8_t depth) {
    return this->list_directory_file_info(path.c_str(), depth);
  }
  
  size_t file_size(const char *path);
  size_t file_size(std::string const &path) {
    return this->file_size(path.c_str());
  }
  
  uint64_t get_used_space();
  uint64_t get_total_space();
  uint64_t get_free_space();

#ifdef USE_SENSOR
  void add_file_size_sensor(sensor::Sensor *sensor, std::string const &path) {
    this->file_size_sensors_.emplace_back(sensor, path);
  }
#endif

  void set_clk_pin(uint8_t pin) { this->clk_pin_ = pin; }
  void set_cmd_pin(uint8_t pin) { this->cmd_pin_ = pin; }
  void set_data0_pin(uint8_t pin) { this->data0_pin_ = pin; }
  void set_data1_pin(uint8_t pin) { this->data1_pin_ = pin; }
  void set_data2_pin(uint8_t pin) { this->data2_pin_ = pin; }
  void set_data3_pin(uint8_t pin) { this->data3_pin_ = pin; }
  void set_mode_1bit(bool mode) { this->mode_1bit_ = mode; }
  void set_power_ctrl_pin(int8_t pin) { this->power_ctrl_pin_ = pin; }

 protected:
  ErrorCode init_error_{NO_ERROR};
  uint8_t clk_pin_{0};
  uint8_t cmd_pin_{0};
  uint8_t data0_pin_{0};
  uint8_t data1_pin_{0};
  uint8_t data2_pin_{0};
  uint8_t data3_pin_{0};
  bool mode_1bit_{false};
  int8_t power_ctrl_pin_{-1};
  
#ifdef USE_ESP_IDF
  sdmmc_card_t *card_{nullptr};
#endif
  
#ifdef USE_SENSOR
  std::vector<FileSizeSensor> file_size_sensors_{};
#endif
  
  void update_sensors();
  
#ifdef USE_ESP32_FRAMEWORK_ARDUINO
  std::string sd_card_type_to_string(int type) const;
#endif
  
#ifdef USE_ESP_IDF
  std::string sd_card_type() const;
#endif
  
  std::vector<FileInfo> &list_directory_file_info_rec(const char *path, uint8_t depth, std::vector<FileInfo> &list);
  
  static std::string error_code_to_string(ErrorCode error) {
    switch (error) {
      case ERR_PIN_SETUP: return "Pin setup error";
      case ERR_MOUNT: return "Mount error";
      case ERR_NO_CARD: return "No card present";
      case NO_ERROR: return "No error";
      default: return "Unknown error";
    }
  }
};

}  // namespace sd_mmc_card
}  // namespace esphome
