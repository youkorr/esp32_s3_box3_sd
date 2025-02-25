#include "sd_card.h"
#include "esphome/core/log.h"
#include <PNGdec.h>
#include <Audio.h>

namespace esphome {
namespace sd_card {

static const char *TAG = "sd_card";

// Objet global pour la lecture audio
Audio audio;

// Objet global pour la lecture d'images PNG
PNG png;
File pngFile;

// Fonction de rappel pour charger les données PNG
int pngRead(PNGFILE *page, unsigned long offset, unsigned char *buffer, unsigned long count) {
    return pngFile.read(buffer, count);
}

// Fonction de rappel pour se déplacer dans le fichier PNG
int pngSeek(PNGFILE *page, unsigned long position) {
    return pngFile.seek(position);
}

void SdMmc::setup() {
  if (!SD_MMC.begin()) {
    ESP_LOGE(TAG, "Failed to mount SD card");
    this->mark_failed();
    return;
  }
  ESP_LOGI(TAG, "SD card mounted successfully");

  // Initialiser la lecture audio
  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT); // Configure les broches I2S
  audio.setVolume(10); // Volume par défaut (0-21)
}

void SdMmc::loop() {
  audio.loop(); // Gère la lecture audio en continu
}

bool SdMmc::play_audio(const char *path) {
  if (!SD_MMC.exists(path)) {
    ESP_LOGE(TAG, "File not found: %s", path);
    return false;
  }
  audio.connecttoFS(SD_MMC, path); // Lire le fichier audio
  return true;
}

bool SdMmc::load_image(const char *path, uint8_t *buffer, size_t buffer_size) {
  pngFile = SD_MMC.open(path, "r");
  if (!pngFile) {
    ESP_LOGE(TAG, "Failed to open file: %s", path);
    return false;
  }

  int result = png.open(pngRead, pngSeek);
  if (result != PNG_SUCCESS) {
    ESP_LOGE(TAG, "Failed to open PNG: %d", result);
    pngFile.close();
    return false;
  }

  // Charger l'image dans le buffer
  png.decode(buffer, 0);
  png.close();
  pngFile.close();
  return true;
}

}  // namespace sd_card
}  // namespace esphome

