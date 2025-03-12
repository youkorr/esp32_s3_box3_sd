#include "sd_file_server.h"
#include "esphome/core/log.h"
#include "esphome/components/network/util.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace sd_file_server {

// Écrit une ligne HTML pour un fichier donné
void SDFileServer::write_row(AsyncResponseStream *response, const sd_mmc_card::FileInfo &info) const {
  response->print("<tr>");
  // Accès direct aux champs 'name' et 'size'
  response->printf("<td>%s</td>", info.name.c_str());
  response->printf("<td>%llu</td>", info.size);  // Utilise %llu pour les grands nombres
  response->print("</tr>");
}

// Gère l'affichage de l'index du répertoire
void SDFileServer::handle_index(AsyncWebServerRequest *request, std::string const &path) const {
  auto *response = request->beginResponseStream("text/html");
  response->print("<html><head><title>SD File Server</title></head><body>");
  response->print("<h1>SD Card Contents</h1>");
  response->print("<table border='1'>");
  response->print("<tr><th>Name</th><th>Size</th></tr>");

  // Vérifie si le chemin est un répertoire valide
  if (!this->sd_mmc_card_->is_directory(path)) {
    response->print("<tr><td colspan='2'>Erreur: Chemin non valide ou introuvable</td></tr>");
    response->print("</table></body></html>");
    request->send(response);
    return;
  }

  // Récupère la liste des fichiers dans le répertoire
  std::vector<sd_mmc_card::FileInfo> files = this->sd_mmc_card_->list_files(path);

  // Affiche chaque fichier
  for (const auto &file : files) {
    this->write_row(response, file);
  }

  response->print("</table></body></html>");
  request->send(response);
}

// Constructeur
SDFileServer::SDFileServer(web_server_base::WebServerBase *base) : base_(base) {}

// Initialisation du serveur de fichiers
void SDFileServer::setup() {
  ESP_LOGCONFIG(TAG, "Setting up SD File Server...");
  this->base_->init();
  this->base_->add_handler(this);
}

// Affichage de la configuration
void SDFileServer::dump_config() {
  ESP_LOGCONFIG(TAG, "SD File Server:");
  ESP_LOGCONFIG(TAG, "  URL Prefix: %s", this->url_prefix_.c_str());
  ESP_LOGCONFIG(TAG, "  Root Path: %s", this->root_path_.c_str());
  ESP_LOGCONFIG(TAG, "  Deletion Enabled: %s", YESNO(this->deletion_enabled_));
  ESP_LOGCONFIG(TAG, "  Download Enabled: %s", YESNO(this->download_enabled_));
  ESP_LOGCONFIG(TAG, "  Upload Enabled: %s", YESNO(this->upload_enabled_));
}

// Vérifie si la requête peut être gérée
bool SDFileServer::canHandle(AsyncWebServerRequest *request) {
  return request->url().rfind(this->build_prefix(), 0) == 0;
}

// Gère la requête HTTP
void SDFileServer::handleRequest(AsyncWebServerRequest *request) {
  std::string path = this->extract_path_from_url(request->url().c_str());
  std::string absolute_path = this->build_absolute_path(path);

  if (request->method() == HTTP_GET) {
    if (path.empty() || this->sd_mmc_card_->is_directory(absolute_path)) {
      this->handle_index(request, path);
    } else {
      this->handle_get(request);
    }
  } else if (request->method() == HTTP_DELETE && this->deletion_enabled_) {
    this->handle_delete(request);
  }
}

// Gère l'upload de fichiers
void SDFileServer::handleUpload(AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len, bool final) {
  // Implémentation de l'upload (à personnaliser selon vos besoins)
}

// Définit le préfixe URL
void SDFileServer::set_url_prefix(std::string const &url_prefix) {
  this->url_prefix_ = url_prefix;
}

// Définit le chemin racine sur la carte SD
void SDFileServer::set_root_path(std::string const &root_path) {
  this->root_path_ = root_path;
}

// Définit l'instance SdMmc
void SDFileServer::set_sd_mmc_card(sd_mmc_card::SdMmc *sd_mmc_card) {
  this->sd_mmc_card_ = sd_mmc_card;
}

// Active/Désactive la suppression de fichiers
void SDFileServer::set_deletion_enabled(bool deletion_enabled) {
  this->deletion_enabled_ = deletion_enabled;
}

// Active/Désactive le téléchargement de fichiers
void SDFileServer::set_download_enabled(bool download_enabled) {
  this->download_enabled_ = download_enabled;
}

// Active/Désactive l'upload de fichiers
void SDFileServer::set_upload_enabled(bool upload_enabled) {
  this->upload_enabled_ = upload_enabled;
}

// Construit le préfixe URL
std::string SDFileServer::build_prefix() const {
  return this->url_prefix_;
}

// Extrait le chemin depuis l'URL
std::string SDFileServer::extract_path_from_url(std::string const &url) const {
  return url.substr(this->build_prefix().length());
}

// Construit le chemin absolu
std::string SDFileServer::build_absolute_path(std::string path) const {
  return Path::join(this->root_path_, path);
}

// Utilities pour manipuler les chemins
std::string Path::file_name(std::string const &path) {
  size_t pos = path.find_last_of('/');
  return pos == std::string::npos ? path : path.substr(pos + 1);
}

bool Path::is_absolute(std::string const &path) {
  return !path.empty() && path[0] == '/';
}

bool Path::trailing_slash(std::string const &path) {
  return !path.empty() && path.back() == '/';
}

std::string Path::join(std::string const &path1, std::string const &path2) {
  if (path1.empty()) return path2;
  if (path2.empty()) return path1;

  std::string result = path1;
  if (!trailing_slash(path1) && !is_absolute(path2)) {
    result += "/";
  }
  result += path2;
  return result;
}

std::string Path::remove_root_path(std::string path, std::string const &root) {
  if (path.rfind(root, 0) == 0) {
    return path.substr(root.length());
  }
  return path;
}

}  // namespace sd_file_server
}  // namespace esphome





