#include "sd_mmc_card.h"
#include "ff.h"
#include "diskio.h"

void SdMmc::setup() {
  if (this->power_ctrl_pin_ != nullptr) {
    gpio_config_t gpio_cfg = {
      .pin_bit_mask = static_cast<uint64_t>(this->power_ctrl_pin_->get_pin()),
      .mode = GPIO_MODE_OUTPUT,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&gpio_cfg);
    gpio_set_level(static_cast<gpio_num_t>(this->power_ctrl_pin_->get_pin()), 0);
    ESP_LOGI(TAG, "SD Card power control enabled on GPIO %d", this->power_ctrl_pin_->get_pin());
  }

  esp_vfs_fat_sdmmc_mount_config_t mount_config = {
      .format_if_mount_failed = false, .max_files = 5, .allocation_unit_size = 16 * 1024};

  sdmmc_host_t host = SDMMC_HOST_DEFAULT();
  sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();

  if (this->mode_1bit_) {
    slot_config.width = 1;
  } else {
    slot_config.width = 4;
  }

#ifdef SOC_SDMMC_USE_GPIO_MATRIX
  slot_config.clk = static_cast<gpio_num_t>(this->clk_pin_);
  slot_config.cmd = static_cast<gpio_num_t>(this->cmd_pin_);
  slot_config.d0 = static_cast<gpio_num_t>(this->data0_pin_);

  if (!this->mode_1bit_) {
    slot_config.d1 = static_cast<gpio_num_t>(this->data1_pin_);
    slot_config.d2 = static_cast<gpio_num_t>(this->data2_pin_);
    slot_config.d3 = static_cast<gpio_num_t>(this->data3_pin_);
  }
#endif

  // Enable internal pullups on enabled pins. The internal pullups
  // are insufficient however, please make sure 10k external pullups are
  // connected on the bus. This is for debug / example purpose only.
  slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

  auto ret = esp_vfs_fat_sdmmc_mount(MOUNT_POINT.c_str(), &host, &slot_config, &mount_config, &this->card_);

  if (ret != ESP_OK) {
    if (ret == ESP_FAIL) {
      this->init_error_ = ErrorCode::ERR_MOUNT;
    } else {
      this->init_error_ = ErrorCode::ERR_NO_CARD;
    }
    mark_failed();
    return;
  }

#ifdef USE_TEXT_SENSOR
  if (this->sd_card_type_text_sensor_ != nullptr)
    this->sd_card_type_text_sensor_->publish_state(sd_card_type());
#endif

  update_sensors();
}

void SdMmc::write_file(const char *path, const uint8_t *buffer, size_t len, const char *mode) {
  std::string absolut_path = build_path(path);
  FILE *file = fopen(absolut_path.c_str(), mode);
  if (file == NULL) {
    ESP_LOGE(TAG, "Failed to open file for writing");
    return;
  }
  size_t bytesWritten = 0;
  while (bytesWritten < len) {
    size_t chunkSize = std::min<size_t>(len - bytesWritten, 1024); // Adjust chunk size as needed
    if (fwrite(buffer + bytesWritten, 1, chunkSize, file) != chunkSize) {
      ESP_LOGE(TAG, "Failed to write to file");
      fclose(file);
      return;
    }
    bytesWritten += chunkSize;
  }
  fclose(file);
  this->update_sensors();
}

bool SdMmc::create_directory(const char *path) {
  ESP_LOGV(TAG, "Create directory: %s", path);
  std::string absolut_path = build_path(path);
  if (mkdir(absolut_path.c_str(), 0777) < 0) {
    ESP_LOGE(TAG, "Failed to create a new directory: %s", strerror(errno));
    return false;
  }
  this->update_sensors();
  return true;
}

bool SdMmc::remove_directory(const char *path) {
  ESP_LOGV(TAG, "Remove directory: %s", path);
  if (!this->is_directory(path)) {
    ESP_LOGE(TAG, "Not a directory");
    return false;
  }
  std::string absolut_path = build_path(path);
  if (remove(absolut_path.c_str()) != 0) {
    ESP_LOGE(TAG, "Failed to remove directory: %s", strerror(errno));
  }
  this->update_sensors();
  return true;
}

bool SdMmc::delete_file(const char *path) {
  ESP_LOGV(TAG, "Delete File: %s", path);
  if (this->is_directory(path)) {
    ESP_LOGE(TAG, "Not a file");
    return false;
  }
  std::string absolut_path = build_path(path);
  if (remove(absolut_path.c_str()) != 0) {
    ESP_LOGE(TAG, "Failed to remove file: %s", strerror(errno));
  }
  this->update_sensors();
  return true;
}

std::vector<uint8_t> SdMmc::read_file(char const *path) {
  ESP_LOGV(TAG, "Read File: %s", path);
  std::string absolut_path = build_path(path);
  
  // Use FATFS file functions instead of SPIFFS
  FIL file;
  FRESULT res = f_open(&file, absolut_path.c_str(), FA_READ);
  if (res != FR_OK) {
    ESP_LOGE(TAG, "File does not exist or cannot be opened: %s (error %d)", path, res);
    return std::vector<uint8_t>();
  }

  // Get file size
  FILINFO finfo;
  res = f_stat(absolut_path.c_str(), &finfo);
  if (res != FR_OK) {
    ESP_LOGE(TAG, "Failed to get file info: %s (error %d)", path, res);
    f_close(&file);
    return std::vector<uint8_t>();
  }
  
  size_t fileSize = finfo.fsize;
  ESP_LOGV(TAG, "Reading file: %zu bytes", fileSize);

  std::vector<uint8_t> result;
  result.resize(fileSize);

  UINT bytesRead;
  res = f_read(&file, result.data(), fileSize, &bytesRead);
  if (res != FR_OK || bytesRead != fileSize) {
    ESP_LOGE(TAG, "Failed to read file: %s (error %d, read %u of %zu)", path, res, bytesRead, fileSize);
    f_close(&file);
    return std::vector<uint8_t>();
  }

  f_close(&file);
  return result;
}

void SdMmc::read_file_by_chunks(const char *path, size_t chunk_size, void (*callback)(const uint8_t *, size_t)) {
  std::string absolut_path = build_path(path);
  
  // Use FATFS file functions instead of SPIFFS
  FIL file;
  FRESULT res = f_open(&file, absolut_path.c_str(), FA_READ);
  if (res != FR_OK) {
    ESP_LOGE(TAG, "File does not exist or cannot be opened: %s (error %d)", path, res);
    return;
  }

  // Get file size
  FILINFO finfo;
  res = f_stat(absolut_path.c_str(), &finfo);
  if (res != FR_OK) {
    ESP_LOGE(TAG, "Failed to get file info: %s (error %d)", path, res);
    f_close(&file);
    return;
  }
  
  size_t fileSize = finfo.fsize;
  if (fileSize == 0) {
    ESP_LOGW(TAG, "File is empty");
    f_close(&file);
    return;
  }

  uint8_t buffer[chunk_size];
  size_t totalBytesRead = 0;
  
  while (totalBytesRead < fileSize) {
    UINT bytesRead;
    res = f_read(&file, buffer, chunk_size, &bytesRead);
    
    if (res != FR_OK) {
      ESP_LOGE(TAG, "Failed to read chunk at offset %zu (error %d)", totalBytesRead, res);
      f_close(&file);
      return;
    }
    
    if (bytesRead == 0) {
      break; // End of file
    }
    
    callback(buffer, bytesRead);
    totalBytesRead += bytesRead;
  }
  
  f_close(&file);
}

std::vector<std::string> SdMmc::list_directory(const char *path, uint8_t depth) {
  std::vector<std::string> list;
  std::vector<FileInfo> infos = list_directory_file_info(path, depth);
  list.resize(infos.size());
  std::transform(infos.cbegin(), infos.cend(), list.begin(), [](FileInfo const &info) { return info.path; });
  return list;
}

std::vector<std::string> SdMmc::list_directory(std::string path, uint8_t depth) {
  return this->list_directory(path.c_str(), depth);
}

std::vector<FileInfo> SdMmc::list_directory_file_info(const char *path, uint8_t depth) {
  std::vector<FileInfo> list;
  list_directory_file_info_rec(path, depth, list);
  return list;
}

std::vector<FileInfo> SdMmc::list_directory_file_info(std::string path, uint8_t depth) {
  return this->list_directory_file_info(path.c_str(), depth);
}

size_t SdMmc::file_size(const char *path) {
  std::string absolut_path = build_path(path);
  
  // Use FATFS file functions instead of SPIFFS
  FILINFO finfo;
  FRESULT res = f_stat(absolut_path.c_str(), &finfo);
  if (res != FR_OK) {
    ESP_LOGE(TAG, "Failed to get file info: %s (error %d)", path, res);
    return -1;
  }
  
  return finfo.fsize;
}

bool SdMmc::is_directory(const char *path) {
  std::string absolut_path = build_path(path);
  
  // Use FATFS file functions instead of SPIFFS
  FILINFO finfo;
  FRESULT res = f_stat(absolut_path.c_str(), &finfo);
  if (res != FR_OK) {
    return false;
  }
  
  return (finfo.fattrib & AM_DIR) != 0;
}

#ifdef USE_SENSOR
void SdMmc::add_file_size_sensor(sensor::Sensor *sensor, std::string const &path) {
  this->file_size_sensors_.emplace_back(sensor, path);
}
#endif

void SdMmc::set_clk_pin(uint8_t pin) { this->clk_pin_ = pin; }

void SdMmc::set_cmd_pin(uint8_t pin) { this->cmd_pin_ = pin; }

void SdMmc::set_data0_pin(uint8_t pin) { this->data0_pin_ = pin; }

void SdMmc::set_data1_pin(uint8_t pin) { this->data1_pin_ = pin; }

void SdMmc::set_data2_pin(uint8_t pin) { this->data2_pin_ = pin; }

void SdMmc::set_data3_pin(uint8_t pin) { this->data3_pin_ = pin; }

void SdMmc::set_mode_1bit(bool b) { this->mode_1bit_ = b; }

void SdMmc::set_power_ctrl_pin(GPIOPin *pin) { this->power_ctrl_pin_ = pin; }

std::string SdMmc::error_code_to_string(SdMmc::ErrorCode code) {
  switch (code) {
    case ErrorCode::ERR_PIN_SETUP:
      return "Failed to set pins";
    case ErrorCode::ERR_MOUNT:
      return "Failed to mount card";
    case ErrorCode::ERR_NO_CARD:
      return "No card found";
    default:
      return "Unknown error";
  }
}

std::vector<FileInfo> &SdMmc::list_directory_file_info_rec(const char *path, uint8_t depth, std::vector<FileInfo> &list) {
  ESP_LOGV(TAG, "Listing directory file info: %s\n", path);
  std::string absolut_path = build_path(path);
  
  // Use FATFS file functions instead of SPIFFS
  DIR dir;
  FILINFO finfo;
  FRESULT res = f_opendir(&dir, absolut_path.c_str());
  if (res != FR_OK) {
    ESP_LOGE(TAG, "Failed to open directory: %s (error %d)", path, res);
    return list;
  }
  
  char entry_path[FILE_PATH_MAX];
  size_t entry_path_len = strlen(path);
  strlcpy(entry_path, path, sizeof(entry_path));
  strlcpy(entry_path + entry_path_len, "/", sizeof(entry_path) - entry_path_len);
  entry_path_len = strlen(entry_path);

  while (true) {
    res = f_readdir(&dir, &finfo);
    if (res != FR_OK || finfo.fname[0] == 0) {
      break; // Error or end of directory
    }
    
    if (strcmp(finfo.fname, ".") == 0 || strcmp(finfo.fname, "..") == 0) {
      continue; // Skip . and .. entries
    }
    
    strlcpy(entry_path + entry_path_len, finfo.fname, sizeof(entry_path) - entry_path_len);
    bool is_dir = (finfo.fattrib & AM_DIR) != 0;
    size_t file_size = is_dir ? 0 : finfo.fsize;
    
    list.emplace_back(entry_path, file_size, is_dir);
    
    if (is_dir && depth > 0) {
      std::string subdir_path = entry_path;
      list_directory_file_info_rec(subdir_path.c_str(), depth - 1, list);
    }
  }
  
  f_closedir(&dir);
  return list;
}

#ifdef USE_ESP_IDF
std::string SdMmc::sd_card_type() const {
  if (this->card_->is_sdio) {
    return "SDIO";
  } else if (this->card_->is_mmc) {
    return "MMC";
  } else {
    return (this->card_->ocr & SD_OCR_SDHC_CAP) ? "SDHC/SDXC" : "SDSC";
  }
  return "UNKNOWN";
}
#endif

void SdMmc::update_sensors() {
#ifdef USE_SENSOR
  if (this->card_ == nullptr)
    return;

  FATFS *fs;
  DWORD fre_clust, fre_sect, tot_sect;
  uint64_t total_bytes = -1, free_bytes = -1, used_bytes = -1;
  auto res = f_getfree(MOUNT_POINT.c_str(), &fre_clust, &fs);
  if (!res) {
    tot_sect = (fs->n_fatent - 2) * fs->csize;
    fre_sect = fre_clust * fs->csize;

    total_bytes = static_cast<uint64_t>(tot_sect) * FF_SS_SDCARD;
    free_bytes = static_cast<uint64_t>(fre_sect) * FF_SS_SDCARD;
    used_bytes = total_bytes - free_bytes;
  }

  if (this->used_space_sensor_ != nullptr)
    this->used_space_sensor_->publish_state(used_bytes);
  if (this->total_space_sensor_ != nullptr)
    this->total_space_sensor_->publish_state(total_bytes);
  if (this->free_space_sensor_ != nullptr)
    this->free_space_sensor_->publish_state(free_bytes);

  for (auto &sensor : this->file_size_sensors_) {
    if (sensor.sensor != nullptr)
      sensor.sensor->publish_state(this->file_size(sensor.path));
  }
#endif
}

}  // namespace sd_mmc_card
}  // namespace esphome

#endif  // USE_ESP_IDF

