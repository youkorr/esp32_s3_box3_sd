#include "sd_file_server.h"
#include "esphome/core/log.h"
#include "esphome/components/sd_mmc_card/sd_mmc_card.h"
#include "esphome/components/web_server_base/web_server_base.h"

namespace esphome {
namespace sd_file_server {

static const char *TAG = "sd_file_server";

SDFileServer::SDFileServer(web_server_base::WebServerBase *base) : base_(base), sd_mmc_card_(nullptr), upload_enabled_(false) {}

void SDFileServer::setup() {
  this->base_->add_handler(this);
  ESP_LOGI(TAG, "SD File Server initialized");
}

void SDFileServer::dump_config() {
  ESP_LOGCONFIG(TAG, "SD File Server configuration:");
  ESP_LOGCONFIG(TAG, "  URL Prefix: %s", this->url_prefix_.c_str());
  ESP_LOGCONFIG(TAG, "  Root Path: %s", this->root_path_.c_str());
  ESP_LOGCONFIG(TAG, "  Upload Enabled: %s", this->upload_enabled_ ? "true" : "false");
}

bool SDFileServer::canHandle(AsyncWebServerRequest *request) {
  // Si la requête correspond au préfixe URL et à la méthode POST, on gère l'upload
  return request->url().startsWith("/" + this->url_prefix_) && request->method() == HTTP_POST;
}

void SDFileServer::handleRequest(AsyncWebServerRequest *request) {
  // Le gestionnaire de requêtes appelle handleUpload si la méthode POST est utilisée pour l'upload de fichiers
  if (this->upload_enabled_ && request->url().startsWith("/" + this->url_prefix_)) {
    request->send(200, "text/plain", "Send file to upload");
  } else {
    request->send(404, "text/plain", "Not Found");
  }
}

void SDFileServer::handleUpload(AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len,
                                bool final) {
  // Vérifier si l'upload est activé
  if (!this->upload_enabled_) {
    request->send(400, "text/plain", "Upload is not enabled");
    return;
  }

  // Création du chemin du fichier sur la carte SD
  std::string file_path = this->build_absolute_path(filename.c_str());

  // Ouvrir le fichier pour écrire
  File file = SD_MMC.open(file_path.c_str(), FILE_WRITE);
  if (!file) {
    ESP_LOGE(TAG, "Failed to open file %s for writing", file_path.c_str());
    request->send(500, "text/plain", "Failed to open file for writing");
    return;
  }

  // Écrire les données dans le fichier
  file.write(data, len);
  file.close();

  // Si l'upload est terminé (final), envoyer la réponse
  if (final) {
    ESP_LOGI(TAG, "File %s uploaded successfully", filename.c_str());
    request->send(200, "text/plain", "File uploaded successfully");
  }
}

void SDFileServer::set_url_prefix(std::string const &url_prefix) {
  this->url_prefix_ = url_prefix;
}

void SDFileServer::set_root_path(std::string const &root_path) {
  this->root_path_ = root_path;
}

void SDFileServer::set_sd_mmc_card(sd_mmc_card::SdMmc *sd_mmc_card) {
  this->sd_mmc_card_ = sd_mmc_card;
}

void SDFileServer::set_deletion_enabled(bool deletion_enabled) {
  this->deletion_enabled_ = deletion_enabled;
}

void SDFileServer::set_download_enabled(bool download_enabled) {
  this->download_enabled_ = download_enabled;
}

void SDFileServer::set_upload_enabled(bool upload_enabled) {
  this->upload_enabled_ = upload_enabled;
}

std::string SDFileServer::build_prefix() const {
  return this->url_prefix_;
}

std::string SDFileServer::extract_path_from_url(std::string const &url) const {
  // Extrait le chemin du fichier à partir de l'URL (supprime le préfixe et les autres parties)
  std::string path = url;
  if (path.find(this->url_prefix_) == 0) {
    path = path.substr(this->url_prefix_.length());
  }
  return path;
}

std::string SDFileServer::build_absolute_path(std::string file_name) const {
  // Construire un chemin absolu pour le fichier
  return this->root_path_ + "/" + file_name;
}

void SDFileServer::write_row(AsyncResponseStream *response, sd_mmc_card::FileInfo const &info) const {
  // Fonction pour écrire une ligne dans la réponse, utilisée pour l'affichage des fichiers
  response->printf("<tr><td>%s</td><td>%s</td><td><a href='/delete/%s'>Delete</a></td></tr>", info.name.c_str(), info.size.c_str(), info.name.c_str());
}

void SDFileServer::handle_index(AsyncWebServerRequest *request, std::string const &path) const {
  // Fonction pour gérer la page d'index (généralement utilisée pour l'affichage de la liste des fichiers)
}

void SDFileServer::handle_get(AsyncWebServerRequest *request) const {
  // Fonction pour gérer les requêtes GET
}

void SDFileServer::handle_delete(AsyncWebServerRequest *request) {
  // Fonction pour gérer les suppressions de fichiers
}

void SDFileServer::handle_download(AsyncWebServerRequest *request, std::string const &file_name) const {
  // Fonction pour gérer le téléchargement de fichiers
}

}  // namespace sd_file_server
}  // namespace esphome
