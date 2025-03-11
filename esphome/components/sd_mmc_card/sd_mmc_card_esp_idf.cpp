#include "sd_mmc_card.h"

#ifdef USE_ESP_IDF

#include "math.h"
#include "esphome/core/log.h"
#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
#include "driver/sdmmc_types.h"
#include "driver/gpio.h"

int constexpr SD_OCR_SDHC_CAP = (1 << 30);

namespace esphome {
namespace sd_mmc_card {

static const char *TAG = "sd_mmc_card_esp_idf";

void SdMmc::setup() {
  sdmmc_host_t host = SDMMC_HOST_DEFAULT();
  sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();

  if (this->mode_1bit_) {
    slot_config.width = 1;
  } else {
    slot_config.width = 4;
  }

  gpio_set_pull_mode((gpio_num_t)this->clk_pin_, GPIO_PULLUP_ONLY);
  gpio_set_pull_mode((gpio_num_t)this->cmd_pin_, GPIO_PULLUP_ONLY);
  gpio_set_pull_mode((gpio_num_t)this->data0_pin_, GPIO_PULLUP_ONLY);
  if (!this->mode_1bit_) {
    gpio_set_pull_mode((gpio_num_t)this->data1_pin_, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode((gpio_num_t)this->data2_pin_, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode((gpio_num_t)this->data3_pin_, GPIO_PULLUP_ONLY);
  }

  esp_err_t ret = sdmmc_host_init();
  if (ret != ESP_OK) {
    this->init_error_ = ERR_PIN_SETUP;
    ESP_LOGE(TAG, "Failed to initialize SDMMC host: %s", esp_err_to_name(ret));
    return;
  }

  ret = sdmmc_host_init_slot(SDMMC_HOST_SLOT_1, &slot_config);
  if (ret != ESP_OK) {
    this->init_error_ = ERR_PIN_SETUP;
    ESP_LOGE(TAG, "Failed to initialize SDMMC slot: %s", esp_err_to_name(ret));
    return;
  }

  sdmmc_card_t *card;
  ret = sdmmc_card_init(&host, &card);
  if (ret != ESP_OK) {
    this->init_error_ = ERR_MOUNT;
    ESP_LOGE(TAG, "Failed to mount SD card: %s", esp_err_to_name(ret));
    return;
  }

  this->card_ = card;
  this->init_error_ = ErrorCode::NO_ERROR;
  ESP_LOGI(TAG, "SD card mounted successfully");
}

void SdMmc::loop() {
  if (this->init_error_ != ErrorCode::NO_ERROR) {
    return;
  }

  this->update_sensors();
}

void SdMmc::dump_config() {
  ESP_LOGCONFIG(TAG, "SD/MMC Card:");
  if (this->init_error_ != ErrorCode::NO_ERROR) {
    ESP_LOGE(TAG, "  Error: %s", error_code_to_string(this->init_error_).c_str());
    return;
  }

  ESP_LOGCONFIG(TAG, "  Card Type: %s", this->sd_card_type().c_str());
  ESP_LOGCONFIG(TAG, "  Total Size: %.2f MB", (double)this->card_->csd.capacity * this->card_->csd.sector_size / (1024.0 * 1024.0));
}

bool SdMmc::read_file_chunk(const char *path, uint8_t *buffer, size_t offset, size_t len) {
  FILE *file = fopen(path, "rb");
  if (!file) {
    ESP_LOGE(TAG, "Failed to open file for reading");
    return false;
  }

  if (fseek(file, offset, SEEK_SET) != 0) {
    ESP_LOGE(TAG, "Failed to seek to position %zu", offset);
    fclose(file);
    return false;
  }

  size_t bytesRead = fread(buffer, 1, len, file);
  fclose(file);
  
  if (bytesRead != len) {
    ESP_LOGE(TAG, "Failed to read %zu bytes (read %zu)", len, bytesRead);
    return false;
  }

  return true;
}

std::vector<uint8_t> SdMmc::read_file(const char *path) {
  std::vector<uint8_t> data;
  FILE *file = fopen(path, "rb");
  if (!file) {
    ESP_LOGE(TAG, "Failed to open file for reading");
    return data;
  }

  fseek(file, 0, SEEK_END);
  size_t size = ftell(file);
  fseek(file, 0, SEEK_SET);
  
  data.resize(size);
  fread(data.data(), 1, size, file);
  fclose(file);
  return data;
}

void SdMmc::write_file(const char *path, const uint8_t *buffer, size_t len, const char *mode) {
  FILE *file = fopen(path, mode);
  if (!file) {
    ESP_LOGE(TAG, "Failed to open file for writing");
    return;
  }

  size_t bytesWritten = fwrite(buffer, 1, len, file);
  fclose(file);

  if (bytesWritten != len) {
    ESP_LOGE(TAG, "Failed to write complete file (wrote %zu/%zu bytes)", bytesWritten, len);
  }
}

bool SdMmc::delete_file(const char *path) {
  if (remove(path) != 0) {
    ESP_LOGE(TAG, "Failed to delete file");
    return false;
  }
  return true;
}

bool SdMmc::create_directory(const char *path) {
  if (mkdir(path, 0755) != 0) {
    ESP_LOGE(TAG, "Failed to create directory");
    return false;
  }
  return true;
}

bool SdMmc::remove_directory(const char *path) {
  if (rmdir(path) != 0) {
    ESP_LOGE(TAG, "Failed to remove directory");
    return false;
  }
  return true;
}

bool SdMmc::is_directory(const char *path) {
  struct stat st;
  if (stat(path, &st) != 0) {
    return false;
  }
  return S_ISDIR(st.st_mode);
}

std::vector<std::string> SdMmc::list_directory(const char *path, uint8_t depth) {
  std::vector<std::string> result;
  DIR *dir = opendir(path);
  if (!dir) {
    return result;
  }

  struct dirent *entry;
  while ((entry = readdir(dir)) != NULL) {
    std::string entry_name = entry->d_name;
    if (entry_name == "." || entry_name == "..") {
      continue;
    }

    result.push_back(entry_name);
    if (entry->d_type == DT_DIR && depth > 0) {
      std::string subdir_path = std::string(path) + "/" + entry_name;
      std::vector<std::string> subdir = this->list_directory(subdir_path.c_str(), depth - 1);
      result.insert(result.end(), subdir.begin(), subdir.end());
    }
  }

  closedir(dir);
  return result;
}

std::vector<FileInfo> SdMmc::list_directory_file_info(const char *path, uint8_t depth) {
  std::vector<FileInfo> result;
  this->list_directory_file_info_rec(path, depth, result);
  return result;
}

std::vector<FileInfo> &SdMmc::list_directory_file_info_rec(const char *path, uint8_t depth, std::vector<FileInfo> &list) {
  DIR *dir = opendir(path);
  if (!dir) {
    return list;
  }

  struct dirent *entry;
  while ((entry = readdir(dir)) != NULL) {
    std::string entry_name = entry->d_name;
    if (entry_name == "." || entry_name == "..") {
      continue;
    }

    std::string full_path = std::string(path) + "/" + entry_name;
    struct stat st;
    if (stat(full_path.c_str(), &st) == 0) {
      list.emplace_back(entry_name, st.st_size, S_ISDIR(st.st_mode));
      if (S_ISDIR(st.st_mode) && depth > 0) {
        this->list_directory_file_info_rec(full_path.c_str(), depth - 1, list);
      }
    }
  }

  closedir(dir);
  return list;
}

size_t SdMmc::file_size(const char *path) {
  struct stat st;
  if (stat(path, &st) != 0) {
    return 0;
  }
  return st.st_size;
}

std::string SdMmc::sd_card_type() const {
  if (!this->card_) {
    return "Unknown";
  }

  switch (this->card_->ocr & SD_OCR_SDHC_CAP) {
    case SD_OCR_SDHC_CAP: return "SDHC";
    default: return "SDSC";
  }
}

void SdMmc::update_sensors() {
#ifdef USE_SENSOR
  if (this->used_space_sensor_ != nullptr) {
    uint64_t total = (uint64_t)this->card_->csd.capacity * this->card_->csd.sector_size;
    uint64_t used = total - this->get_free_space();
    this->used_space_sensor_->publish_state(used / (1024.0 * 1024.0));
  }
  if (this->total_space_sensor_ != nullptr) {
    uint64_t total = (uint64_t)this->card_->csd.capacity * this->card_->csd.sector_size;
    this->total_space_sensor_->publish_state(total / (1024.0 * 1024.0));
  }
  if (this->free_space_sensor_ != nullptr) {
    this->free_space_sensor_->publish_state(this->get_free_space() / (1024.0 * 1024.0));
  }
#endif

#ifdef USE_TEXT_SENSOR
  if (this->sd_card_type_text_sensor_ != nullptr) {
    this->sd_card_type_text_sensor_->publish_state(this->sd_card_type());
  }
#endif
}

uint64_t SdMmc::get_free_space() {
  if (!this->card_) {
    return 0;
  }

  FATFS *fs;
  DWORD fre_clust;
  if (f_getfree("0:", &fre_clust, &fs) != FR_OK) {
    return 0;
  }

  return (uint64_t)fre_clust * fs->csize * this->card_->csd.sector_size;
}

}  // namespace sd_mmc_card
}  // namespace esphome

#endif  // USE_ESP_IDF
