#pragma once
#include "esphome/core/gpio.h"
#include "esphome/core/defines.h"
#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include <map>
#include <string>

namespace esphome {
namespace sd_mmc_card {

struct FileInfo {
  std::string path;
  size_t size;
  bool is_directory;

  FileInfo(std::string const &path, size_t size, bool is_directory);
};

class SdMmc : public Component {
 public:
  enum class ErrorCode {
    NONE,
    ERR_PIN_SETUP,
    ERR_MOUNT,
    ERR_NO_CARD
  };

  void set_clk_pin(uint8_t pin);
  void set_cmd_pin(uint8_t pin);
  void set_data0_pin(uint8_t pin);
  void set_data1_pin(uint8_t pin);
  void set_data2_pin(uint8_t pin);
  void set_data3_pin(uint8_t pin);
  void set_mode_1bit(bool b);

  void setup() override;
  void loop() override;
  void dump_config() override;

  bool is_setup() const { return this->is_setup_; }
  bool is_failed() const { return this->init_error_ != ErrorCode::NONE; }

  // File operations
  void write_file(const char *path, const uint8_t *buffer, size_t len);
  void append_file(const char *path, const uint8_t *buffer, size_t len);
  std::vector<uint8_t> read_file(const char *path);
  bool delete_file(const char *path);
  size_t file_size(const char *path);
  bool is_directory(const char *path);

  // Directory operations
  std::vector<std::string> list_directory(const char *path, uint8_t depth = 0);
  std::vector<FileInfo> list_directory_file_info(const char *path, uint8_t depth = 0);

  // Streamed file API
  int open_file(const char* path, const char* mode = "r");
  size_t read_chunk(int file_handle, uint8_t* buffer, size_t chunk_size);
  void close_file(int file_handle);
  size_t get_file_size(int file_handle) const;

  #ifdef USE_SENSOR
  class FileSizeSensor {
   public:
    FileSizeSensor(sensor::Sensor *sensor, std::string const &path);
    sensor::Sensor *sensor;
    std::string path;
  };

  void add_file_size_sensor(sensor::Sensor *sensor, std::string const &path);
  #endif

  static std::string error_code_to_string(ErrorCode code);
  static long double convertBytes(uint64_t value, MemoryUnits unit);

 protected:
  struct OpenFile {
    void* file_ptr;
    size_t file_size;
  };

  void list_directory_file_info_rec(const char *path, uint8_t depth, std::vector<FileInfo> &list);
  void write_file(const char *path, const uint8_t *buffer, size_t len, const char *mode);
  std::string build_path(const char *path) const;

  bool mode_1bit_{false};
  uint8_t clk_pin_{};
  uint8_t cmd_pin_{};
  uint8_t data0_pin_{};
  uint8_t data1_pin_{};
  uint8_t data2_pin_{};
  uint8_t data3_pin_{};
  
  std::map<int, OpenFile> open_files_;
  int next_file_handle_{0};
  ErrorCode init_error_{ErrorCode::NONE};
  bool is_setup_{false};

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

}  // namespace sd_mmc_card
}  // namespace esphome
