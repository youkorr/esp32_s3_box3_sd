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

std::vector<uint8_t> SdMmc::read_file(char const *path) {
  ESP_LOGV(TAG, "Read File: %s", path);
  std::string absolut_path = build_path(path);
  FILE *file = nullptr;
  file = fopen(absolut_path.c_str(), "rb");
  if (file == nullptr) {
    ESP_LOGE(TAG, "Failed to open file for reading");
    return std::vector<uint8_t>();
  }

  const size_t buffer_size = 512; // Un secteur SD standard
  std::vector<uint8_t> buffer(buffer_size);
  std::vector<uint8_t> res;
  
  size_t total_read = 0;
  size_t bytes_read = 0;
  
  while ((bytes_read = fread(buffer.data(), 1, buffer_size, file)) > 0) {
    res.insert(res.end(), buffer.data(), buffer.data() + bytes_read);
    total_read += bytes_read;
    
    // Option: Afficher la progression de la lecture
    if (total_read % 4096 == 0) {
      ESP_LOGV(TAG, "Read progress: %d bytes", total_read);
    }
  }
  
  fclose(file);
  return res;
}

void SdMmc::write_file_chunked(const char *path, const uint8_t *buffer, size_t len, size_t chunk_size) {
  std::string absolut_path = build_path(path);
  FILE *file = nullptr;
  file = fopen(absolut_path.c_str(), "a");
  if (file == nullptr) {
    ESP_LOGE(TAG, "Failed to open file for chunked writing");
    return;
  }

  const size_t max_chunk = 512; // Un secteur SD standard
  chunk_size = std::min(chunk_size, max_chunk);
  
  size_t written = 0;
  while (written < len) {
    size_t to_write = std::min(chunk_size, len - written);
    size_t chunk_written = fwrite(buffer + written, 1, to_write, file);
    if (chunk_written != to_write) {
      ESP_LOGE(TAG, "Failed to write chunk to file: wrote %d of %d bytes", chunk_written, to_write);
      break;
    }
    written += chunk_written;
    
    fflush(file);
    
    // Option: Afficher la progression de l'écriture
    if (written % 4096 == 0) {
      ESP_LOGV(TAG, "Write progress: %d/%d bytes", written, len);
    }
  }

  fclose(file);
  this->update_sensors();
}

std::vector<SdMmc::FileInfo> &SdMmc::list_directory_file_info_rec(const char *path, uint8_t depth, std::vector<FileInfo> &list) {
  ESP_LOGV(TAG, "Listing directory file info: %s\n", path);
  std::string absolut_path = build_path(path);
  DIR *dir = opendir(absolut_path.c_str());
  if (!dir) {
    ESP_LOGE(TAG, "Failed to open directory: %s", strerror(errno));
    return list;
  }

  char entry_absolut_path[FILE_PATH_MAX] = {0};
  char entry_path[FILE_PATH_MAX] = {0};
  const size_t dirpath_len = MOUNT_POINT.size();
  
  strlcpy(entry_path, path, sizeof(entry_path));
  size_t entry_path_len = strlen(entry_path);
  
  if (entry_path_len > 0 && entry_path[entry_path_len-1] != '/') {
    if (entry_path_len + 2 >= FILE_PATH_MAX) {
      ESP_LOGE(TAG, "Path too long");
      closedir(dir);
      return list;
    }
    entry_path[entry_path_len] = '/';
    entry_path[entry_path_len + 1] = '\0';
    entry_path_len++;
  }
  
  strlcpy(entry_absolut_path, MOUNT_POINT.c_str(), sizeof(entry_absolut_path));

  struct dirent *entry;
  list.reserve(list.size() + 16); // Réserver un peu d'espace pour éviter les réallocations fréquentes
  
  while ((entry = readdir(dir)) != nullptr) {
    // Ignorer les entrées "." et ".."
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
      continue;
    }
    
    // Vérifier la longueur du nom avant de concaténer
    if (entry_path_len + strlen(entry->d_name) >= FILE_PATH_MAX ||
        dirpath_len + strlen(path) + 1 + strlen(entry->d_name) >= FILE_PATH_MAX) {
      ESP_LOGE(TAG, "Path too long for entry: %s", entry->d_name);
      continue;
    }
    
    // Construire le chemin complet
    strcpy(entry_path + entry_path_len, entry->d_name);
    sprintf(entry_absolut_path, "%s%s", MOUNT_POINT.c_str(), entry_path);
    
    size_t file_size = 0;
    if (entry->d_type != DT_DIR) {
      struct stat info;
      if (stat(entry_absolut_path, &info) == 0) {
        file_size = info.st_size;
      } else {
        ESP_LOGE(TAG, "Failed to stat file: %s", strerror(errno));
      }
    }
    
    // Ajouter l'entrée à la liste
    list.emplace_back(entry_path, file_size, entry->d_type == DT_DIR);
    
    // Récursion pour les sous-répertoires si nécessaire
    if (entry->d_type == DT_DIR && depth > 0) {
      // Conserver le chemin actuel
      char current_path[FILE_PATH_MAX];
      strcpy(current_path, entry_path);
      
      // Récursion
      list_directory_file_info_rec(current_path, depth - 1, list);
      
      // Restaurer le chemin parent après la récursion
      strcpy(entry_path + entry_path_len, "");
    } else {
      // Réinitialiser le chemin pour la prochaine itération
      entry_path[entry_path_len] = '\0';
    }
  }
  
  closedir(dir);
  return list;
}

std::vector<uint8_t> SdMmc::read_file_chunked(char const *path, size_t offset, size_t chunk_size) {
  std::string absolut_path = build_path(path);
  FILE *file = nullptr;
  file = fopen(absolut_path.c_str(), "rb");
  if (file == nullptr) {
    ESP_LOGE(TAG, "Failed to open file for chunked reading");
    return std::vector<uint8_t>();
  }

  if (fseek(file, offset, SEEK_SET) != 0) {
    ESP_LOGE(TAG, "Failed to seek to position");
    fclose(file);
    return std::vector<uint8_t>();
  }

  const size_t max_chunk_size = 1024; // Ajustez selon vos besoins
  size_t actual_chunk_size = std::min(chunk_size, max_chunk_size);
  
  std::vector<uint8_t> res;
  res.resize(actual_chunk_size);
  
  size_t read = fread(res.data(), 1, actual_chunk_size, file);
  if (read < actual_chunk_size) {
    res.resize(read); // Redimensionner pour n'utiliser que l'espace nécessaire
  }

  fclose(file);
  return res;
}

}  // namespace sd_mmc_card
}  // namespace esphome
#endif  // USE_ESP_IDF




