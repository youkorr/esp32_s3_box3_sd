#include "sd_mmc_card.h"
#include "freertos/task.h"

namespace esphome {
namespace sd_mmc_card {

bool SdMmc::write_file_chunked(const char *path, size_t chunk_size,
                               std::function<std::vector<uint8_t>(size_t)> data_provider) {
  ESP_LOGV(TAG, "Writing file in chunks: %s", path);

  std::string absolut_path = build_path(path);
  FILE *file = fopen(absolut_path.c_str(), "wb");
  if (!file) {
    ESP_LOGE(TAG, "Failed to open file for writing: %s", strerror(errno));
    return false;
  }

  size_t total_written = 0;
  size_t chunk_index = 0;

  while (true) {
    std::vector<uint8_t> chunk = data_provider(chunk_index++);
    if (chunk.empty()) {
      break; // Fin des données
    }

    size_t bytes_written = fwrite(chunk.data(), 1, chunk.size(), file);
    if (bytes_written != chunk.size()) {
      ESP_LOGE(TAG, "Failed to write chunk: wrote %zu of %zu bytes", bytes_written, chunk.size());
      fclose(file);
      return false;
    }

    total_written += bytes_written;
    fflush(file); // Vider le tampon périodiquement
    vTaskDelay(pdMS_TO_TICKS(1)); // Céder le contrôle à d'autres tâches
    ESP_LOGV(TAG, "Write progress: %zu bytes", total_written);
  }

  fclose(file);
  this->update_sensors();
  ESP_LOGI(TAG, "File written successfully: %zu bytes", total_written);
  return true;
}

bool SdMmc::read_file_chunked(const char *path, size_t chunk_size,
                              std::function<void(const std::vector<uint8_t> &)> data_consumer) {
  ESP_LOGV(TAG, "Reading file in chunks: %s", path);

  std::string absolut_path = build_path(path);
  FILE *file = fopen(absolut_path.c_str(), "rb");
  if (!file) {
    ESP_LOGE(TAG, "Failed to open file for reading: %s", strerror(errno));
    return false;
  }

  std::vector<uint8_t> buffer(chunk_size);
  size_t total_read = 0;

  while (size_t bytes_read = fread(buffer.data(), 1, chunk_size, file)) {
    std::vector<uint8_t> chunk(buffer.begin(), buffer.begin() + bytes_read);
    data_consumer(chunk); // Passer le morceau au consumer
    total_read += bytes_read;

    vTaskDelay(pdMS_TO_TICKS(1)); // Céder le contrôle à d'autres tâches
    ESP_LOGV(TAG, "Read progress: %zu bytes", total_read);
  }

  fclose(file);
  ESP_LOGI(TAG, "File read successfully: %zu bytes", total_read);
  return true;
}

}  // namespace sd_mmc_card
}  // namespace esphome

