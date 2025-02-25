#include "sd_mmc_card.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

#include <FS.h>
#include <SPI.h>
#include <SD.h>
#include <SD_MMC.h>

#endif

namespace esphome {
namespace sd_mmc_card {

static const char *TAG = "sd_mmc_card";

void SdMmc::setup() {
  ESP_LOGCONFIG(TAG, "Setting up SD MMC Card...");

  // Initialize SD Card
  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);

  if (!SD.begin(SD_CS, SPI)) {
    ESP_LOGE(TAG, "Card Mount Failed");
    return;
  }

  uint8_t cardType = SD.cardType();
  if (cardType == CARD_NONE) {
    ESP_LOGE(TAG, "No SD card attached");
    return;
  }

  ESP_LOGI(TAG, "SD Card Type: %s", SD.cardTypeName(cardType));
  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  ESP_LOGI(TAG, "SD Card Size: %lluMB", cardSize);

  this->update_sensors();
}

void SdMmc::loop() { this->update_sensors(); }

void SdMmc::dump_config() {
  ESP_LOGCONFIG(TAG, "SD MMC Card:");
  ESP_LOGCONFIG(TAG, "  CLK Pin: %u", this->clk_pin_);
  ESP_LOGCONFIG(TAG, "  CMD Pin: %u", this->cmd_pin_);
  ESP_LOGCONFIG(TAG, "  Data0 Pin: %u", this->data0_pin_);
  ESP_LOGCONFIG(TAG, "  Mode 1-bit: %s", YESNO(this->mode_1bit_));
}

void SdMmc::set_clk_pin(uint8_t clk_pin) { this->clk_pin_ = clk_pin; }

void SdMmc::set_cmd_pin(uint8_t cmd_pin) { this->cmd_pin_ = cmd_pin; }

void SdMmc::set_data0_pin(uint8_t data0_pin) { this->data0_pin_ = data0_pin; }

void SdMmc::set_data1_pin(uint8_t data1_pin) { this->data1_pin_ = data1_pin; }

void SdMmc::set_data2_pin(uint8_t data2_pin) { this->data2_pin_ = data2_pin; }

void SdMmc::set_data3_pin(uint8_t data3_pin) { this->data3_pin_ = data3_pin; }

void SdMmc::set_mode_1bit(bool mode_1bit) { this->mode_1bit_ = mode_1bit; }

void SdMmc::write_file(const char *path, const uint8_t *buffer, size_t len, const char *mode) {}
void SdMmc::write_file(const char *path, const uint8_t *buffer, size_t len) {}
void SdMmc::append_file(const char *path, const uint8_t *buffer, size_t len) {}
bool SdMmc::delete_file(const char *path) { return true; }
bool SdMmc::delete_file(std::string const &path) { return true; }
bool SdMmc::create_directory(const char *path) { return true; }
bool SdMmc::remove_directory(const char *path) { return true; }
std::vector SdMmc::read_file(char const *path) { return {}; }
std::vector SdMmc::read_file(std::string const &path) { return {}; }
bool SdMmc::is_directory(const char *path) { return false; }
bool SdMmc::is_directory(std::string const &path) { return false; }
std::vector SdMmc::list_directory(const char *path, uint8_t depth) { return {}; }
std::vector SdMmc::list_directory(std::string path, uint8_t depth) { return {}; }
std::vector SdMmc::list_directory_file_info(const char *path, uint8_t depth) { return {}; }
std::vector SdMmc::list_directory_file_info(std::string path, uint8_t depth) { return {}; }
size_t SdMmc::file_size(const char *path) { return 0; }
size_t SdMmc::file_size(std::string const &path) { return 0; }

void SdMmc::update_sensors() {
   if (SD.cardType() == CARD_NONE) {
    ESP_LOGE(TAG, "No SD card attached");
    return;
  }
}

bool SdMmc::sdcard_is_mounted() {
  return SD.cardType() != CARD_NONE;
}

}  // namespace sd_mmc_card
}  // namespace esphome

