#ifndef SD_CARD_H
#define SD_CARD_H

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include <vector>
#include <string>
#include <driver/sdmmc_host.h>
#include <driver/sdspi_host.h>
#include <sdmmc_cmd.h>
#include <esp_vfs_fat.h>

namespace esphome {
namespace sd_card {

// Déclaration de la structure FileInfo
struct FileInfo {
  std::string path;
  size_t size;
  bool is_directory;

  FileInfo(std::string const &path, size_t size, bool is_directory)
      : path(path), size(size), is_directory(is_directory) {}
};

class SdMmc : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;

  // Méthodes pour la lecture audio
  bool play_audio(const char *path);

  // Méthodes pour la lecture d'images
  bool load_image(const char *path, uint8_t *buffer, size_t buffer_size);

  // Méthode pour vérifier si la carte SD est montée
  bool is_mounted() const { return mounted_; }

  // Méthodes existantes pour la gestion des fichiers
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

  void set_cs_pin(uint8_t pin) { cs_pin_ = pin; }
  void set_mosi_pin(uint8_t pin) { mosi_pin_ = pin; }
  void set_miso_pin(uint8_t pin) { miso_pin_ = pin; }
  void set_clk_pin(uint8_t pin) { clk_pin_ = pin; }

  enum class ErrorCode {
    ERR_PIN_SETUP,
    ERR_MOUNT,
    ERR_NO_CARD,
  };

  static std::string error_code_to_string(ErrorCode code);

#ifdef USE_SENSOR
  void add_file_size_sensor(sensor::Sensor *sensor, std::string const &path);
#endif

 protected:
  void write_file(const char *path, const uint8_t *buffer, size_t len, const char *mode);
  void list_directory_file_info_rec(const char *path, uint8_t depth, std::vector<FileInfo> &list);

  uint8_t cs_pin_;
  uint8_t mosi_pin_;
  uint8_t miso_pin_;
  uint8_t clk_pin_;
  bool mounted_ = false;

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


