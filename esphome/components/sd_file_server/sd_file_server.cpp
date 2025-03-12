#include "sd_file_server.h"
#include "esphome/core/log.h"
#include "esphome/components/network/util.h"
#include "esphome/core/helpers.h"
#include "esphome/components/sd_mmc_card/sd_mmc_card.h"  // Ajoutez cette inclusion si nécessaire

namespace esphome {
namespace sd_file_server {

void SDFileServer::write_row(AsyncResponseStream *response, const sd_mmc_card::FileInfo &info) const {
  response->print("<tr>");
  // Utilisez les méthodes de FileInfo (ex: name(), size())
  response->printf("<td>%s</td>", info.name().c_str());  // Méthode 'name()'
  response->printf("<td>%d</td>", info.size());          // Méthode 'size()'
  response->print("</tr>");
}

void SDFileServer::handle_index(AsyncWebServerRequest *request, std::string const &path) const {
  auto *response = request->beginResponseStream("text/html");
  response->print("<html><head><title>SD File Server</title></head><body>");
  response->print("<h1>SD Card Contents</h1>");
  response->print("<table border='1'>");
  response->print("<tr><th>Name</th><th>Size</th></tr>");

  // Listing des fichiers via l'API SDMmc
  std::vector<sd_mmc_card::FileInfo> files;
  sd_mmc_card::Dir dir = this->sd_mmc_card_->openDir(path);  // Ouvre le répertoire

  if (!dir) {
    response->print("<tr><td colspan='2'>Erreur: Répertoire introuvable</td></tr>");
    response->print("</table></body></html>");
    request->send(response);
    return;
  }

  while (dir.readDir()) {  // Lit chaque entrée
    files.push_back(dir.getFileInfo());
  }

  for (const auto &file : files) {
    this->write_row(response, file);
  }

  response->print("</table></body></html>");
  request->send(response);
}

SDFileServer::SDFileServer(web_server_base::WebServerBase *base) : base_(base) {}

void SDFileServer::setup() {
  ESP_LOGCONFIG(TAG, "Setting up SD File Server...");
  this->base_->init();
  this->base_->add_handler(this);
}

void SDFileServer::dump_config() {
  ESP_LOGCONFIG(TAG, "SD File Server:");
  ESP_LOGCONFIG(TAG, "  URL Prefix: %s", this->url_prefix_.c_str());
  ESP_LOGCONFIG(TAG, "  Root Path: %s", this->root_path_.c_str());
  ESP_LOGCONFIG(TAG, "  Deletion Enabled: %s", YESNO(this->deletion_enabled_));
  ESP_LOGCONFIG(TAG, "  Download Enabled: %s", YESNO(this->download_enabled_));
  ESP_LOGCONFIG(TAG, "  Upload Enabled: %s", YESNO(this->upload_enabled_));
}

bool SDFileServer::canHandle(AsyncWebServerRequest *request) {
  return request->url().rfind(this->build_prefix(), 0) == 0;
}

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

void SDFileServer::handleUpload(AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len, bool final) {
  // Implémentation de l'upload
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
  return url.substr(this->build_prefix().length());
}

std::string SDFileServer::build_absolute_path(std::string path) const {
  return Path::join(this->root_path_, path);
}

// Gestion des chemins (Path utilities)
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






