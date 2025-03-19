#include "sd_file_server.h"
#include "esphome/core/log.h"

namespace esphome {
namespace sd_file_server {

static const char *TAG = "sd_file_server";

// Définition du type pour le callback de fournisseur de données
using DataProviderCallback = std::function<size_t(uint8_t*, size_t)>;

/**
 * Écrit un fichier par morceaux en utilisant un callback pour récupérer les données
 * 
 * @param path          Chemin absolu du fichier sur la carte SD
 * @param data_provider Fonction qui fournit les prochaines données à écrire
 * @param buffer_size   Taille du tampon à utiliser (par défaut 4096 octets)
 * @return              true si succès, false si échec
 */
bool SDFileServer::write_file_chunked(const std::string &path, DataProviderCallback data_provider, size_t buffer_size) {
  if (sd_mmc_card_ == nullptr) {
    ESP_LOGE(TAG, "SD card not initialized");
    return false;
  }

  // Valider la taille du tampon (min 512 octets, max 8192 octets)
  if (buffer_size < 512) buffer_size = 512;
  if (buffer_size > 8192) buffer_size = 8192;

  // Allouer le tampon dans la PSRAM si disponible, sinon dans la mémoire normale
  uint8_t *buffer = nullptr;
#ifdef CONFIG_SPIRAM_SUPPORT
  if (heap_caps_get_free_size(MALLOC_CAP_SPIRAM) > buffer_size) {
    buffer = (uint8_t *)heap_caps_malloc(buffer_size, MALLOC_CAP_SPIRAM);
    ESP_LOGD(TAG, "Allocated chunk buffer in SPIRAM");
  }
#endif
  
  // Si pas de PSRAM ou si l'allocation en PSRAM a échoué, essayer l'allocation normale
  if (buffer == nullptr) {
    buffer = (uint8_t *)malloc(buffer_size);
    ESP_LOGD(TAG, "Allocated chunk buffer in internal memory");
  }

  if (buffer == nullptr) {
    ESP_LOGE(TAG, "Failed to allocate buffer for chunked write");
    return false;
  }

  // Créer le fichier
  ESP_LOGD(TAG, "Opening file for chunked write: %s", path.c_str());
  FILE *file = sd_mmc_card_->open_file(path.c_str(), "wb");
  if (file == nullptr) {
    ESP_LOGE(TAG, "Failed to create file for chunked write");
    free(buffer);
    return false;
  }

  bool success = true;
  size_t total_bytes = 0;
  
  // Boucle principale de traitement par morceaux
  while (true) {
    // Récupérer le prochain morceau de données via le callback
    size_t bytes_read = data_provider(buffer, buffer_size);
    if (bytes_read == 0) {
      // Plus de données à lire, fin de l'opération avec succès
      break;
    }

    // Écrire le morceau dans le fichier
    size_t bytes_written = fwrite(buffer, 1, bytes_read, file);
    if (bytes_written != bytes_read) {
      ESP_LOGE(TAG, "Write error: %d bytes written out of %d", bytes_written, bytes_read);
      success = false;
      break;
    }

    total_bytes += bytes_written;
    
    // Log toutes les 32 Ko pour suivre la progression
    if (total_bytes % (32 * 1024) == 0) {
      ESP_LOGD(TAG, "Chunked write progress: %d KB", total_bytes / 1024);
    }

    // Vider (flush) les données sur la carte SD régulièrement
    // pour éviter d'accumuler trop de données en mémoire tampon
    if (total_bytes % (buffer_size * 4) == 0) {
      fflush(file);
      ESP_LOGV(TAG, "Flushed data to SD card");
    }

    // Petit délai pour permettre aux autres tâches de s'exécuter
    // et éviter un watchdog timer reset
    if (total_bytes % (buffer_size * 8) == 0) {
      delay(1);
    }
  }

  // Fermer le fichier et libérer le tampon
  fflush(file);
  fclose(file);
  free(buffer);

  ESP_LOGI(TAG, "Chunked write complete: %d bytes written to %s", total_bytes, path.c_str());
  return success;
}

// Fonction d'aide pour la lecture par morceaux
using DataConsumerCallback = std::function<bool(const uint8_t*, size_t)>;

/**
 * Lit un fichier par morceaux et passe chaque morceau à un callback
 * 
 * @param path          Chemin absolu du fichier sur la carte SD
 * @param data_consumer Fonction qui traite les données lues
 * @param buffer_size   Taille du tampon à utiliser (par défaut 4096 octets)
 * @return              true si succès, false si échec
 */
bool SDFileServer::read_file_chunked(const std::string &path, DataConsumerCallback data_consumer, size_t buffer_size) {
  if (sd_mmc_card_ == nullptr) {
    ESP_LOGE(TAG, "SD card not initialized");
    return false;
  }

  // Valider la taille du tampon
  if (buffer_size < 512) buffer_size = 512;
  if (buffer_size > 8192) buffer_size = 8192;

  // Allouer le tampon préférablement en PSRAM
  uint8_t *buffer = nullptr;
#ifdef CONFIG_SPIRAM_SUPPORT
  buffer = (uint8_t *)heap_caps_malloc(buffer_size, MALLOC_CAP_SPIRAM);
#endif
  
  if (buffer == nullptr) {
    buffer = (uint8_t *)malloc(buffer_size);
  }

  if (buffer == nullptr) {
    ESP_LOGE(TAG, "Failed to allocate buffer for chunked read");
    return false;
  }

  // Ouvrir le fichier en lecture
  FILE *file = sd_mmc_card_->open_file(path.c_str(), "rb");
  if (file == nullptr) {
    ESP_LOGE(TAG, "Failed to open file for chunked read: %s", path.c_str());
    free(buffer);
    return false;
  }

  bool success = true;
  size_t total_bytes = 0;
  
  // Boucle de lecture
  while (!feof(file)) {
    size_t bytes_read = fread(buffer, 1, buffer_size, file);
    if (bytes_read == 0) {
      // Fin du fichier ou erreur
      if (ferror(file)) {
        ESP_LOGE(TAG, "Error reading file");
        success = false;
      }
      break;
    }

    // Appeler le callback avec les données lues
    if (!data_consumer(buffer, bytes_read)) {
      ESP_LOGW(TAG, "Data consumer requested to stop");
      break;
    }

    total_bytes += bytes_read;
    
    // Log la progression
    if (total_bytes % (32 * 1024) == 0) {
      ESP_LOGD(TAG, "Chunked read progress: %d KB", total_bytes / 1024);
    }
    
    // Petit délai pour permettre aux autres tâches de s'exécuter
    if (total_bytes % (buffer_size * 8) == 0) {
      delay(1);
    }
  }

  // Nettoyer
  fclose(file);
  free(buffer);

  ESP_LOGI(TAG, "Chunked read complete: %d bytes read from %s", total_bytes, path.c_str());
  return success;
}

}  // namespace sd_file_server
}  // namespace esphome





















