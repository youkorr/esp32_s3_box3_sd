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

void SdMmc::write_file(const char *path, const uint8_t *buffer, size_t len, const char *mode) {
  ESP_LOGD(TAG, "Writing file to SD card: %s", path);
  File file = SD.open(path, FILE_WRITE);
  if (!file) {
    ESP_LOGE(TAG, "Failed to open file for writing");
    return;
  }
  file.write(buffer, len);
  file.close();
}

void SdMmc::write_file(const char *path, const uint8_t *buffer, size_t len) {
  this->write_file(path, buffer, len, "w");
}

void SdMmc::append_file(const char *path, const uint8_t *buffer, size_t len) {
  ESP_LOGD(TAG, "Appending file to SD card: %s", path);
  File file = SD.open(path, FILE_APPEND);
  if (!file) {
    ESP_LOGE(TAG, "Failed to open file for appending");
    return;
  }
  file.write(buffer, len);
  file.close();
}

bool SdMmc::delete_file(const char *path) {
  ESP_LOGD(TAG, "Deleting file from SD card: %s", path);
  if (SD.remove(path)) {
    ESP_LOGD(TAG, "File deleted");
    return true;
  } else {
    ESP_LOGE(TAG, "Failed to delete file");
    return false;
  }
}

bool SdMmc::delete_file(std::string const &path) {
  return this->delete_file(path.c_str());
}

bool SdMmc::create_directory(const char *path) {
  ESP_LOGD(TAG, "Creating directory on SD card: %s", path);
  if (SD.mkdir(path)) {
    ESP_LOGD(TAG, "Directory created");
    return true;
  } else {
    ESP_LOGE(TAG, "Failed to create directory");
    return false;
  }
}

bool SdMmc::remove_directory(const char *path) {
  ESP_LOGD(TAG, "Removing directory from SD card: %s", path);
  if (SD.rmdir(path)) {
    ESP_LOGD(TAG, "Directory removed");
    return true;
  } else {
    ESP_LOGE(TAG, "Failed to remove directory");
    return false;
  }
}

std::vector<uint8_t> SdMmc::read_file(char const *path) {
  ESP_LOGD(TAG, "Reading file from SD card: %s", path);
  File file = SD.open(path);
  if (!file) {
    ESP_LOGE(TAG, "Failed to open file for reading");
    return {};
  }
  std::vector<uint8_t> data;
  while (file.available()) {
    data.push_back(file.read());
  }
  file.close();
  return data;
}

std::vector<uint8_t> SdMmc::read_file(std::string const &path) {
  return this->read_file(path.c_str());
}

bool SdMmc::is_directory(const char *path) {
  File file = SD.open(path);
  if (!file) {
    return false;
  }
  bool result = file.isDirectory();
  file.close();
  return result;
}

bool SdMmc::is_directory(std::string const &path) {
  return this->is_directory(path.c_str());
}

std::vector<std::string> SdMmc::list_directory(const char *path, uint8_t depth) {
  std::vector<std::string> list;
  this->list_directory_recursive(path, depth, 0, list);
  return list;
}

std::vector<std::string> SdMmc::list_directory(std::string path, uint8_t depth) {
  return this->list_directory(path.c_str(), depth);
}

void SdMmc::list_directory_recursive(const char *path, uint8_t max_depth, uint8_t current_depth, std::vector<std::string> &list) {
  if (current_depth > max_depth) {
    return;
  }
  File dir = SD.open(path);
  if (!dir) {
    ESP_LOGE(TAG, "Failed to open directory: %s", path);
    return;
  }
  while (true) {
    File entry = dir.openNextFile();
    if (!entry) {
      // no more files
      break;
    }
    String filename = entry.name();
    String filepath = String(path) + "/" + filename;
    list.push_back(filepath.c_str());
    if (entry.isDirectory() && current_depth < max_depth) {
      this->list_directory_recursive(filepath.c_str(), max_depth, current_depth + 1, list);
    }
    entry.close();
  }
  dir.close();
}

std::vector<FileInfo> SdMmc::list_directory_file_info(const char *path, uint8_t depth) {
    std::vector<FileInfo> list;
    this->list_directory_file_info_recursive(path, depth, 0, list);
    return list;
}

std::vector<FileInfo> SdMmc::list_directory_file_info(std::string path, uint8_t depth) {
    return this->list_directory_file_info(path.c_str(), depth);
}

void SdMmc::list_directory_file_info_recursive(const char *path, uint8_t max_depth, uint8_t current_depth, std::vector<FileInfo> &list) {
    if (current_depth > max_depth) {
        return;
    }
    File dir = SD.open(path);
    if (!dir) {
        ESP_LOGE(TAG, "Failed to open directory: %s", path);
        return;
    }
    while (true) {
        File entry = dir.openNextFile();
        if (!entry) {
            // no more files
            break;
        }
        String filename = entry.name();
        String filepath = String(path) + "/" + filename;
        FileInfo file_info(filepath.c_str(), entry.size(), entry.isDirectory());
        list.push_back(file_info);
        if (entry.isDirectory() && current_depth < max_depth) {
            this->list_directory_file_info_recursive(filepath.c_str(), max_depth, current_depth + 1, list);
        }
        entry.close();
    }
    dir.close();
}

size_t SdMmc::file_size(const char *path) {
  File file = SD.open(path);
  if (!file) {
    ESP_LOGE(TAG, "Failed to open file: %s", path);
    return 0;
  }
  size_t size = file.size();
  file.close();
  return size;
}

size_t SdMmc::file_size(std::string const &path) {
  return this->file_size(path.c_str());
}


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





