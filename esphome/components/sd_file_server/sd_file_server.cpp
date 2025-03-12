#include "sd_file_server.h"
#include "esphome/core/log.h"
#include "esphome/components/network/util.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace sd_file_server {

static const char *TAG = "sd_file_server";

// Écrit une ligne HTML pour un fichier donné
void SDFileServer::write_row(AsyncResponseStream *response, const sd_mmc_card::FileInfo &info) const {
  response->print("<tr>");
  // Utilise les champs 'name' et 'size' directement
  response->printf("<td>%s</td>", info.name.c_str());
  response->printf("<td>%llu</td>", info.size);  // Utilise %llu pour les grands nombres
  response->print("</tr>");
}

// Gère l'affichage de l'index du répertoire
void SDFileServer::handle_index(AsyncWebServerRequest *request, std::string const &path) const {
  AsyncResponseStream *response = request->beginResponseStream("text/html");
  response->print("<html><head><title>SD Card Contents</title></head><body>");
  response->print("<h1>SD Card Contents</h1>");
  response->print("<table border='1'>");
  response->print("<tr><th>Name</th><th>Size</th></tr>");

  // Récupère la liste des fichiers dans le répertoire
  auto entries = this->sd_mmc_card_->list_directory_file_info(path, 0);

  for (const auto &entry : entries) {
    this->write_row(response, entry);
  }

  response->print("</table></body></html>");
  request->send(response);
}

// Constructeur
SDFileServer::SDFileServer(web_server_base::WebServerBase *base) : base_(base) {}

// Initialisation du serveur de fichiers
void SDFileServer::setup() {
  ESP_LOGCONFIG(TAG, "Setting up SD File Server...");
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
  return str_startswith(std::string(request->url().c_str()), this->build_prefix());
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
void SDFileServer::handleUpload(AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data,
                                size_t len, bool final) {
  if (!this->upload_enabled_) {
    request->send(401, "application/json", "{ \"error\": \"file upload is disabled\" }");
    return;
  }

  std::string extracted = this->extract_path_from_url(request->url().c_str());
  std::string path = this->build_absolute_path(extracted);

  if (index == 0 && !this->sd_mmc_card_->is_directory(path)) {
    request->send(401, "application/json", "{ \"error\": \"invalid upload folder\" }");
    return;
  }

  std::string file_name(filename.c_str());
  if (index == 0) {
    this->sd_mmc_card_->write_file(Path::join(path, file_name).c_str(), data, len);
  } else {
    this->sd_mmc_card_->append_file(Path::join(path, file_name).c_str(), data, len);
  }

  if (final) {
    request->send(201, "text/html", "Upload successful");
  }
}

// Définit le préfixe URL
void SDFileServer::set_url_prefix(std::string const &prefix) { this->url_prefix_ = prefix; }

// Définit le chemin racine sur la carte SD
void SDFileServer::set_root_path(std::string const &path) { this->root_path_ = path; }

// Définit l'instance SdMmc
void SDFileServer::set_sd_mmc_card(sd_mmc_card::SdMmc *card) { this->sd_mmc_card_ = card; }

// Active/Désactive la suppression de fichiers
void SDFileServer::set_deletion_enabled(bool allow) { this->deletion_enabled_ = allow; }

// Active/Désactive le téléchargement de fichiers
void SDFileServer::set_download_enabled(bool allow) { this->download_enabled_ = allow; }

// Active/Désactive l'upload de fichiers
void SDFileServer::set_upload_enabled(bool allow) { this->upload_enabled_ = allow; }

// Construit le préfixe URL
std::string SDFileServer::build_prefix() const {
  return this->url_prefix_.empty() ? "/" : "/" + this->url_prefix_;
}

// Extrait le chemin depuis l'URL
std::string SDFileServer::extract_path_from_url(std::string const &url) const {
  return url.substr(this->build_prefix().length());
}

// Construit le chemin absolu
std::string SDFileServer::build_absolute_path(std::string relative_path) const {
  return Path::join(this->root_path_, relative_path);
}

// Utilities pour manipuler les chemins
std::string Path::file_name(std::string const &path) {
  size_t pos = path.rfind(Path::separator);
  return pos != std::string::npos ? path.substr(pos + 1) : path;
}

bool Path::is_absolute(std::string const &path) { return !path.empty() && path[0] == Path::separator; }

bool Path::trailing_slash(std::string const &path) { return !path.empty() && path.back() == Path::separator; }

std::string Path::join(std::string const &first, std::string const &second) {
  std::string result = first;
  if (!trailing_slash(first) && !is_absolute(second)) {
    result += Path::separator;
  }
  return result + second;
}

std::string Path::remove_root_path(std::string path, std::string const &root) {
  if (path.rfind(root, 0) == 0) {
    return path.substr(root.length());
  }
  return path;
}

}  // namespace sd_file_server
}  // namespace esphome





