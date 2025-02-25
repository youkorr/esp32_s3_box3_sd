#ifndef SD_CARD_H
#define SD_CARD_H

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include <vector>
#include <string>

namespace esphome {
namespace sd_card {

class SdMmc : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;

  void write_file(const char *path, const uint8_t *buffer, size_t len);
  void append_file(const char *path, const uint8_t *buffer, size_t len);
  std::vector<std::string> list_directory(const char *path, uint8_t depth = 0);
  std::vector<std::string> list_directory(std::string path, uint8_t depth = 0);
  std::vector<FileInfo> list_directory_file_info(const char *path, uint8_t depth = 0);
  std::vector<FileInfo> list_directory_file_info(std::string path, uint8_t depth = 0);
  size_t file_size(std::string const &path);
  bool is_directory(std::string const &path);
  bool delete_file(std::string const &path);
  std::vector<uint8_t> read_file(std::string const &path);

  void set_clk_pin(uint8_t pin) { clk_pin_ = pin; }
  void set_cmd_pin(uint8_t pin) { cmd_pin_ = pin; }
  void set_data0_pin(uint8_t pin) { data0_pin_ = pin; }
  void set_data1_pin(uint8_t pin) { data1_pin_ = pin; }
  void set_data2_pin(uint8_t pin) { data2_pin_ = pin; }
  void set_data3_pin(uint8_t pin) { data3_pin_ = pin; }
  void set_mode_1bit(bool b) { mode_1bit_ = b; }

  enum class ErrorCode {
    ERR_PIN_SETUP,
    ERR_MOUNT,
    ERR_NO_CARD,
  };

  static std::string error_code_to_string(ErrorCode code);

  struct FileInfo {
    std::string path;
    size_t size;
    bool is_directory;

    FileInfo(std::string const &path, size_t size, bool is_directory);
  };

#ifdef USE_SENSOR
  void add_file_size_sensor(sensor::Sensor *sensor, std::string const &path);
#endif

 protected:
  void write_file(const char *path, const uint8_t *buffer, size_t len, const char *mode);
  void list_directory_file_info_rec(const char *path, uint8_t depth, std::vector<FileInfo> &list);

  uint8_t clk_pin_;
  uint8_t cmd_pin_;
  uint8_t data0_pin_;
  uint8_t data1_pin_;
  uint8_t data2_pin_;
  uint8_t data3_pin_;
  bool mode_1bit_ = false;

#ifdef USE_SENSOR
  struct FileSizeSensor {
    sensor::Sensor *sensor;
    std::string path;

    FileSizeSensor(sensor::Sensor *sensor, std::string const &path);
  };

  std::vector<FileSizeSensor> file_size_sensors_;
#endif
};

}  // namespace sd_card
}  // namespace esphome

#endif  // SD_CARD_H


