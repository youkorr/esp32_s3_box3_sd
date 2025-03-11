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

enum class MemoryUnits { BYTES, KILOBYTES, MEGABYTES, GIGABYTES };

enum class ErrorCode {
  NONE = 0,
  ERR_PIN_SETUP,
  ERR_MOUNT,
  ERR_NO_CARD,
};

struct FileInfo {
  std::string path;
  size_t size;
  bool is_directory;
  FileInfo(std::string const &path, size_t size, bool is_directory) 
    : path(path), size(size), is_directory(is_directory) {}
};

#ifdef USE_SENSOR
class FileSizeSensor {
 public:
  FileSizeSensor(sensor::Sensor *sensor, std::string const &path) 
    : sensor(sensor), path(path) {}
  sensor::Sensor *sensor;
  std::string path;
};
#endif

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
  size_t file_size(const char *path);
  bool is_directory(std::string const &path);
  bool is_directory(const char *path);
  bool delete_file(std::string const &path);
  bool delete_file(const char *path);
  std::vector<uint8_t> read_file(std::string const &path);
  std::vector<uint8_t> read_file(const char *path);
  bool read_file_chunked(const char *path, std::function<bool(uint8_t *, size_t, size_t)> callback, size_t chunk_size = 512);

  void set_clk_pin(uint8_t pin);
  void set_cmd_pin(uint8_t pin);
  void set_data0_pin(uint8_t pin);
  void set_data1_pin(uint8_t pin);
  void set_data2_pin(uint8_t pin);
  void set_data3_pin(uint8_t pin);
  void set_mode_1bit(bool b);

#ifdef USE_SENSOR
  void add_file_size_sensor(sensor::Sensor *sensor, std::string const &path);
#endif

  static std::string error_code_to_string(ErrorCode code);

 protected:
  void write_file(const char *path, const uint8_t *buffer, size_t len, const char *mode);
  void list_directory_file_info_rec(const char *path, uint8_t depth, std::vector<FileInfo> &list);
  std::string build_path(const char *path);

  uint8_t clk_pin_;
  uint8_t cmd_pin_;
  uint8_t data0_pin_;
  uint8_t data1_pin_;
  uint8_t data2_pin_;
  uint8_t data3_pin_;
  bool mode_1bit_ = false;
  ErrorCode init_error_ = ErrorCode::NONE;

#ifdef USE_SENSOR
  sensor::Sensor *used_space_sensor_{nullptr};
  sensor::Sensor *total_space_sensor_{nullptr};
  sensor::Sensor *free_space_sensor_{nullptr};
  std::vector<FileSizeSensor> file_size_sensors_;
#endif

#ifdef USE_TEXT_SENSOR
  text_sensor::TextSensor *sd_card_type_text_sensor_{nullptr};
#endif
};

long double convertBytes(uint64_t value, MemoryUnits unit);

}  // namespace sd_mmc_card
}  // namespace esphome
