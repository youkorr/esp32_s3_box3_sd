#include "sd_mmc_card.h"

#ifdef USE_ESP_IDF

#include "math.h"
#include "esphome/core/log.h"
#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
#include "driver/sdmmc_types.h"

int constexpr SD_OCR_SDHC_CAP = (1 << 30);

namespace esphome {
namespace sd_mmc_card {

static constexpr size_t FILE_PATH_MAX = ESP_VFS_PATH_MAX + CONFIG_SPIFFS_OBJ_NAME_LEN;
static const char *TAG = "sd_mmc_card";
static const std::string MOUNT_POINT("/sdcard");

std::string build_path(const char *path) { return MOUNT_POINT + path; }

void SdMmc::setup() {
  if (this->power_ctrl_pin_ != nullptr) {
    this->power_ctrl_pin_->setup();
  }

  esp_vfs_fat_sdmmc_mount_config_t mount_config = {
      .format_if_mount_failed = false,
      .max_files = 5,
      .allocation_unit_size = 16 * 1024
  };
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
  if (this->sd_card_type_text_sensor_ != nullptr) {
    this->sd_card_type_text_sensor_->publish_state(this->sd_card_type());
  }
#endif
  update_sensors();
}

bool SdMmc::write_file(const char *path, std::function<size_t(uint8_t *, size_t)> data_provider, size_t chunk_size) {
  ESP_LOGD(TAG, "Writing to file: %s", path);
  std::string absolute_path = build_path(path);
  FILE *file = fopen(absolute_path.c_str(), "wb"); // Open in write binary mode
  if (file == nullptr) {
    ESP_LOGE(TAG, "Failed to open file for writing: %s", path);
    return false;
  }

  std::vector<uint8_t> buffer(chunk_size);
  size_t bytes_written;
  while (true) {
    size_t bytes_to_write = data_provider(buffer.data(), chunk_size);
    if (bytes_to_write == 0) {
      // No more data
      break;
    }

    bytes_written = fwrite(buffer.data(), 1, bytes_to_write, file);
    if (bytes_written != bytes_to_write) {
      ESP_LOGE(TAG, "Failed to write to file: %s, wrote %zu/%zu", path, bytes_written, bytes_to_write);
      fclose(file);
      return false;
    }
    ESP_LOGVV(TAG, "Wrote chunk of %zu bytes to file", bytes_written);
  }

  fclose(file);
  ESP_LOGD(TAG, "Finished writing to file: %s", path);
  return true;
}

bool SdMmc::read_file(const char *path, std::function<bool(const uint8_t*, size_t)> data_consumer, size_t chunk_size) {
  ESP_LOGD(TAG, "Reading file: %s", path);
  std::string absolute_path = build_path(path);
  FILE *file = fopen(absolute_path.c_str(), "rb"); // Open in read binary mode
  if (file == nullptr) {
    ESP_LOGE(TAG, "Failed to open file for reading: %s", path);
    return false;
  }

  std::vector<uint8_t> buffer(chunk_size);
  size_t bytes_read;
  while ((bytes_read = fread(buffer.data(), 1, chunk_size, file)) > 0) {
    if (!data_consumer(buffer.data(), bytes_read)) {
      ESP_LOGD(TAG, "Data consumer returned false, stopping reading");
      break;
    }
     ESP_LOGVV(TAG, "Read chunk of %zu bytes from file", bytes_read);
  }

  fclose(file);
  ESP_LOGD(TAG, "Finished reading file: %s", path);
  return true;
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
    return false;
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
    return false;
  }

  this->update_sensors();
  return true;
}

bool SdMmc::is_directory(const char *path) {
  std::string absolut_path = build_path(path);
  struct stat info;
  if (stat(absolut_path.c_str(), &info) == 0 && (info.st_mode & S_IFDIR)) {
    return true;
  } else {
    return false;
  }
}

bool SdMmc::is_directory(std::string const &path) {
  return is_directory(path.c_str());
}

std::vector<SdMmc::FileInfo> &SdMmc::list_directory_file_info_rec(const char *path, uint8_t depth,
                                                                  std::vector<FileInfo> &list) {
  ESP_LOGV(TAG, "Listing directory file info: %s\n", path);
  std::string absolut_path = build_path(path);
  DIR *dir = opendir(absolut_path.c_str());
  if (!dir) {
    ESP_LOGE(TAG, "Failed to open directory: %s", strerror(errno));
    return list;
  }

  char entry_absolut_path[FILE_PATH_MAX];
  char entry_path[FILE_PATH_MAX];
  const size_t dirpath_len = MOUNT_POINT.size();
  size_t entry_path_len = strlen(path);
  strlcpy(entry_path, path, sizeof(entry_path));
  strlcpy(entry_path + entry_path_len, "/", sizeof(entry_path) - entry_path_len);
  entry_path_len = strlen(entry_path);
  strlcpy(entry_absolut_path, MOUNT_POINT.c_str(), sizeof(entry_absolut_path));

  struct dirent *entry;
  while ((entry = readdir(dir)) != nullptr) {
    strlcpy(entry_path + entry_path_len, entry->d_name, sizeof(entry_path) - entry_path_len);
    strlcpy(entry_absolut_path + dirpath_len, entry_path, sizeof(entry_absolut_path) - dirpath_len);
    size_t file_size = 0;
    if (entry->d_type != DT_DIR) {
      struct stat info;
      if (stat(entry_absolut_path, &info) < 0) {
        ESP_LOGE(TAG, "Failed to stat file: %s", strerror(errno));
      } else {
        file_size = info.st_size;
      }
    }
    list.emplace_back(entry_path, file_size, entry->d_type == DT_DIR);

    if (entry->d_type == DT_DIR && depth) {
      list_directory_file_info_rec(entry_absolut_path, depth - 1, list);
    }
  }
  closedir(dir);
  return list;
}

size_t SdMmc::file_size(const char *path) {
  std::string absolut_path = build_path(path);
  struct stat info;
  if (stat(absolut_path.c_str(), &info) != 0) {
    ESP_LOGE(TAG, "Failed to stat file: %s", strerror(errno));
    return 0;
  }
  return info.st_size;
}

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

    if (this->used_space_sensor_ != nullptr)
      this->used_space_sensor_->publish_state(used_bytes);
    if (this->total_space_sensor_ != nullptr)
      this->total_space_sensor_->publish_state(total_bytes);
    if (this->free_space_sensor_ != nullptr)
      this->free_space_sensor_->publish_state(free_bytes);
  }
  for (auto &sensor : this->file_size_sensors_) {
    if (sensor.sensor != nullptr) {
      sensor.sensor->publish_state(this->file_size(sensor.path.c_str()));
    }
  }
#endif
}

void SdMmc::add_file_size_sensor(sensor::Sensor *sensor, std::string const &path) {
  this->file_size_sensors_.push_back({sensor, path});
}

}  // namespace sd_mmc_card
}  // namespace esphome
#endif  // USE_ESP_IDF


