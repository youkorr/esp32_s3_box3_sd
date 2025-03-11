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

int constexpr SD_OCR_SDHC_CAP = (1 << 30);  // value defined in esp-idf/components/sdmmc/include/sd_protocol_defs.h

namespace esphome {
namespace sd_mmc_card {

static constexpr size_t FILE_PATH_MAX = ESP_VFS_PATH_MAX + CONFIG_SPIFFS_OBJ_NAME_LEN;
static const char *TAG = "sd_mmc_card";
static const std::string MOUNT_POINT("/sdcard");

std::string build_path(const char *path) { return MOUNT_POINT + path; }

bool SdMmc::read_file_chunked(const char *path, std::function<void(const uint8_t *buffer, size_t len)> callback, size_t chunk_size) {
  std::string absolut_path = build_path(path);
  FILE *file = fopen(absolut_path.c_str(), "rb");
  if (!file) {
    ESP_LOGE(TAG, "Failed to open file for reading");
    return false;
  }

  uint8_t *buffer = new uint8_t[chunk_size];
  size_t bytesRead = 0;

  while ((bytesRead = fread(buffer, 1, chunk_size, file)) > 0) {
    callback(buffer, bytesRead);
  }

  delete[] buffer;
  fclose(file);
  return true;
}

}  // namespace sd_mmc_card
}  // namespace esphome

#endif  // USE_ESP_IDF
