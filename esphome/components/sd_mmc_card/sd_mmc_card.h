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

#include <functional> // Required for std::function

namespace esphome {
namespace sd_mmc_card {

enum MemoryUnits : short { Byte = 0, KiloByte = 1, MegaByte = 2, GigaByte = 3, TeraByte = 4, PetaByte = 5 };

#ifdef USE_SENSOR
struct FileSizeSensor {
  sensor::Sensor *sensor{nullptr};
  std::string path;
  FileSizeSensor() = default;
  FileSizeSensor(sensor::Sensor *, std::string const &path);
};
#endif

struct FileInfo {
  std::string path;
  size_t size;
  bool is_directory;
  FileInfo(std::string const &, size_t, bool);
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
  };

  void setup() override;
  void loop() override;
  void dump_config() override;
  void write_file(const char *path, const uint8_t *buffer, size_t len, const char *mode);
  void write_file(const char *path, const uint8_t *buffer, size_t len);
  void append_file(const char *path, const uint8_t *buffer, size_t len);

  // Modified write_file_chunked function
  void write_file_chunked(const char *path, std::function<size_t(uint8_t *, size_t)> data_provider, size_t chunk_size);

  bool delete_file(const char *path);
  bool delete_file(std::string const &path);
  bool create_directory(const char *path);
  bool remove_directory(const char *path);
  std::vector<uint8_t> read_file(char const *path);
  std::vector<uint8_t> read_file(std::string const &path);
  std::vector<uint8_t> read_file_chunked(char const *path, size_t offset, size_t chunk_size);
  std::vector<uint8_t> read_file_chunked(std::string const &path, size_t offset, size_t chunk_size);
  bool is_directory(const char *path);
  bool is_directory(std::string const &path);
  std::vector<std::string> list_directory(const char *path, uint8_t depth);
  std::vector<std::string> list_directory(std::string path, uint8_t depth);
  std::vector<FileInfo> list_directory_file_info(const char *path, uint8_t depth);
  std::vector<FileInfo> list_directory_file_info(std::string path, uint8_t depth);
  size_t file_size(const char *path);
  size_t file_size(std::string const &path);

#ifdef USE_SENSOR
  void add_file_size_sensor(sensor::Sensor *sensor, std::string const &path);
#endif

  void set_clk_pin(uint8_t pin) { this->clk_pin_ = pin; }
  void set_cmd_pin(uint8_t pin) { this->cmd_pin_ = pin; }
  void set_data0_pin(uint8_t pin) { this->data0_pin_ = pin; }
  void set_data1_pin(uint8_t pin) { this->data1_pin_ = pin; }
  void set_data2_pin(uint8_t pin) { this->data2_pin_ = pin; }
  void set_data3_pin(uint8_t pin) { this->data3_pin_ = pin; }
  void set_mode_1bit(bool b) { this->mode_1bit_ = b; }
  void set_power_ctrl_pin(GPIOPin *pin) { this->power_ctrl_pin_ = pin; }

  std::string error_code_to_string(ErrorCode code);

 protected:
  uint8_t clk_pin_{0};
  uint8_t cmd_pin_{0};
  uint8_t data0_pin_{0};
  uint8_t data1_pin_{0};
  uint8_t data2_pin_{0};
  uint8_t data3_pin_{0};
  bool mode_1bit_{false};
  GPIOPin *power_ctrl_pin_{nullptr};
  ErrorCode init_error_{ErrorCode::ERR_PIN_SETUP};

#ifdef USE_ESP_IDF
  sdmmc_card_t *card_{nullptr};
#endif

  std::string build_path(const char *path);
  std::vector<FileInfo> &list_directory_file_info_rec(const char *path, uint8_t depth, std::vector<FileInfo> &list);
  void update_sensors();
};

long double convertBytes(uint64_t value, MemoryUnits unit);

FileInfo::FileInfo(std::string const &path, size_t size, bool is_directory)
    : path(path), size(size), is_directory(is_directory) {}

}  // namespace sd_mmc_card
}  // namespace esphome

