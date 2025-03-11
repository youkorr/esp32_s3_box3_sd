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

static constexpr size_t FILE_PATH_MAX = ESP_VFS_PATH_MAX + CONFIG_SPIFFS_OBJ_NAME_LEN;
static const char *TAG = "sd_mmc_card";
static const std::string MOUNT_POINT("/sdcard");

std::string build_path(const char *path) { return MOUNT_POINT + path; }

void SdMmc::setup() {
  // ... (conserver la configuration existante)
}

void SdMmc::write_file(const char *path, const uint8_t *buffer, size_t len, const char *mode) {
  // ... (conserver l'implémentation existante)
}

bool SdMmc::create_directory(const char *path) {
  // ... (conserver l'implémentation existante)
}

bool SdMmc::remove_directory(const char *path) {
  // ... (conserver l'implémentation existante)
}

bool SdMmc::delete_file(const char *path) {
  // ... (conserver l'implémentation existante)
}

std::vector<uint8_t> SdMmc::read_file(char const *path) {
  // ... (conserver l'implémentation existante)
}

std::vector<FileInfo> &SdMmc::list_directory_file_info_rec(const char *path, uint8_t depth,
                                                           std::vector<FileInfo> &list) {
  // ... (conserver l'implémentation existante)
}

bool SdMmc::is_directory(const char *path) {
  // ... (conserver l'implémentation existante)
}

size_t SdMmc::file_size(const char *path) {
  // ... (conserver l'implémentation existante)
}

std::string SdMmc::sd_card_type() const {
  // ... (conserver l'implémentation existante)
}

void SdMmc::update_sensors() {
  // ... (conserver l'implémentation existante)
}

// ******** NOUVELLE FONCTION ********
bool SdMmc::read_file_stream(const char *path, uint8_t *buffer, 
                            size_t buffer_size, uint32_t offset) {
  std::string absolut_path = build_path(path);
  FILE *file = fopen(absolut_path.c_str(), "rb");
  if (!file) {
    ESP_LOGE(TAG, "Échec ouverture fichier: %s", path);
    return false;
  }

  if (fseek(file, offset, SEEK_SET) != 0) {
    ESP_LOGE(TAG, "Échec seek fichier: %s", path);
    fclose(file);
    return false;
  }

  size_t bytes_read = fread(buffer, 1, buffer_size, file);
  fclose(file);

  return bytes_read == buffer_size;
}

}  // namespace sd_mmc_card
}  // namespace esphome

#endif  // USE_ESP_IDF
