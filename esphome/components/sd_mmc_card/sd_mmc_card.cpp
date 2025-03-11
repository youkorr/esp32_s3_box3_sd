#include "sd_mmc_card.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
#include "driver/gpio.h"
#include "esp_log.h"

namespace esphome {
namespace sd_mmc_card {

static const char *TAG = "sd_mmc_card";
static const std::string MOUNT_POINT = "/sdcard";

void SdMmc::setup() {
  if (this->is_failed())
    return;

  // Configuration du host SDMMC
  sdmmc_host_t host = SDMMC_HOST_DEFAULT();
  host.slot = SDMMC_HOST_SLOT_1;
  host.max_freq_khz = SDMMC_FREQ_HIGHSPEED;

  // Configuration des pins
  sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
  slot_config.clk = this->clk_pin_;
  slot_config.cmd = this->cmd_pin_;
  slot_config.d0 = this->data0_pin_;
  slot_config.d1 = this->data1_pin_;
  slot_config.d2 = this->data2_pin_;
  slot_config.d3 = this->data3_pin_;
  slot_config.width = this->mode_1bit_ ? 1 : 4;

  // Configuration du montage
  esp_vfs_fat_sdmmc_mount_config_t mount_config = {
    .format_if_mount_failed = false,
    .max_files = 5,
    .allocation_unit_size = 16 * 1024
  };

  // Initialisation
  esp_err_t ret = esp_vfs_fat_sdmmc_mount(
    MOUNT_POINT.c_str(),
    &host,
    &slot_config,
    &mount_config,
    &this->card_
  );

  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "SDMMC mount failed: %s", esp_err_to_name(ret));
    this->mark_failed();
    return;
  }

  // Activation de l'alimentation si configuré
  if (this->power_ctrl_pin_ >= 0) {
    gpio_set_level(static_cast<gpio_num_t>(this->power_ctrl_pin_), 1);
  }

  this->update_sensors();
}

bool SdMmc::read_file_stream(const char *path, uint8_t *buffer, size_t buffer_size, uint32_t offset) {
  std::string full_path = MOUNT_POINT + std::string(path);
  FILE *f = fopen(full_path.c_str(), "rb");
  
  if (!f) {
    ESP_LOGE(TAG, "Failed to open file: %s", full_path.c_str());
    return false;
  }

  fseek(f, 0, SEEK_END);
  size_t file_size = ftell(f);
  if (offset >= file_size) {
    ESP_LOGE(TAG, "Offset exceeds file size");
    fclose(f);
    return false;
  }

  fseek(f, offset, SEEK_SET);
  size_t bytes_read = fread(buffer, 1, buffer_size, f);
  fclose(f);

  return bytes_read == buffer_size;
}

void SdMmc::write_file(const char *path, const uint8_t *buffer, size_t len, const char *mode) {
  std::string full_path = MOUNT_POINT + std::string(path);
  FILE *f = fopen(full_path.c_str(), mode);
  if (f) {
    fwrite(buffer, 1, len, f);
    fclose(f);
    this->update_sensors();
  }
}

bool SdMmc::delete_file(const char *path) {
  std::string full_path = MOUNT_POINT + std::string(path);
  if (unlink(full_path.c_str()) == 0) {
    this->update_sensors();
    return true;
  }
  return false;
}

bool SdMmc::create_directory(const char *path) {
  std::string full_path = MOUNT_POINT + std::string(path);
  if (mkdir(full_path.c_str(), 0777) == 0) {
    this->update_sensors();
    return true;
  }
  return false;
}

bool SdMmc::remove_directory(const char *path) {
  std::string full_path = MOUNT_POINT + std::string(path);
  if (rmdir(full_path.c_str()) == 0) {
    this->update_sensors();
    return true;
  }
  return false;
}

std::vector<uint8_t> SdMmc::read_file(const char *path) {
  std::string full_path = MOUNT_POINT + std::string(path);
  FILE *f = fopen(full_path.c_str(), "rb");
  if (!f) return {};

  fseek(f, 0, SEEK_END);
  size_t size = ftell(f);
  fseek(f, 0, SEEK_SET);

  std::vector<uint8_t> buffer(size);
  fread(buffer.data(), 1, size, f);
  fclose(f);

  return buffer;
}

std::vector<FileInfo> &SdMmc::list_directory_file_info_rec(const char *path, uint8_t depth, std::vector<FileInfo> &list) {
  std::string full_path = MOUNT_POINT + std::string(path);
  DIR *dir = opendir(full_path.c_str());
  if (!dir) return list;

  struct dirent *entry;
  while ((entry = readdir(dir))) {
    std::string entry_name = entry->d_name;
    if (entry_name == "." || entry_name == "..") continue;

    std::string entry_path = std::string(path) + "/" + entry_name;
    std::string full_entry_path = MOUNT_POINT + entry_path;

    struct stat st;
    if (stat(full_entry_path.c_str(), &st) != 0) continue;

    bool is_dir = S_ISDIR(st.st_mode);
    list.emplace_back(entry_path, st.st_size, is_dir);

    if (is_dir && depth > 0) {
      list_directory_file_info_rec(entry_path.c_str(), depth - 1, list);
    }
  }
  closedir(dir);
  return list;
}

size_t SdMmc::file_size(const char *path) {
  std::string full_path = MOUNT_POINT + std::string(path);
  struct stat st;
  if (stat(full_path.c_str(), &st) == 0) {
    return st.st_size;
  }
  return 0;
}

void SdMmc::update_sensors() {
  if (!card_) return;

  // Exemple pour le capteur de type de carte
  #ifdef USE_TEXT_SENSOR
  if (this->sd_card_type_text_sensor_) {
    std::string type = sdmmc_card_type_to_string(card_->type);
    this->sd_card_type_text_sensor_->publish_state(type);
  }
  #endif

  // Mise à jour des capteurs de stockage
  FATFS *fs;
  DWORD fre_clust;
  uint64_t total_sectors, free_sectors;

  if (f_getfree(MOUNT_POINT.c_str(), &fre_clust, &fs) == FR_OK) {
    total_sectors = (fs->n_fatent - 2) * fs->csize;
    free_sectors = fre_clust * fs->csize;

    uint64_t total_bytes = total_sectors * FF_SS_SDCARD;
    uint64_t free_bytes = free_sectors * FF_SS_SDCARD;

    #ifdef USE_SENSOR
    if (this->total_space_sensor_) {
      this->total_space_sensor_->publish_state(total_bytes);
    }
    if (this->free_space_sensor_) {
      this->free_space_sensor_->publish_state(free_bytes);
    }
    #endif
  }
}

}  // namespace sd_mmc_card
}  // namespace esphome
