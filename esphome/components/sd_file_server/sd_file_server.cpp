#include "sd_file_server.h"
#include "esphome/core/log.h"
#include "esphome/components/network/util.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace sd_file_server {

static const char *TAG = "sd_file_server";

SDFileServer::SDFileServer(web_server_base::WebServerBase *base) : base_(base) {}

void SDFileServer::setup() {
  this->base_->add_handler(this);

  // Désactivation des fonctionnalités redondantes
  this->set_deletion_enabled(false);  // Désactiver la suppression
  this->set_download_enabled(false);  // Désactiver le téléchargement
  this->set_upload_enabled(false);    // Désactiver le téléchargement
}

void SDFileServer::dump_config() {
  ESP_LOGCONFIG(TAG, "SD File Server:");
  ESP_LOGCONFIG(TAG, "  Address: %s:%u", network::get_use_address().c_str(), this->base_->get_port());
  ESP_LOGCONFIG(TAG, "  Url Prefix: %s", this->url_prefix_.c_str());
  ESP_LOGCONFIG(TAG, "  Root Path: %s", this->root_path_.c_str());
  ESP_LOGCONFIG(TAG, "  Deletation Enabled: %s", TRUEFALSE(this->deletion_enabled_));
  ESP_LOGCONFIG(TAG, "  Download Enabled : %s", TRUEFALSE(this->download_enabled_));
  ESP_LOGCONFIG(TAG, "  Upload Enabled : %s", TRUEFALSE(this->upload_enabled_));
}

bool SDFileServer::canHandle(AsyncWebServerRequest *request) {
  return true;  // Accepte toutes les requêtes pour redirection
}

void SDFileServer::handleRequest(AsyncWebServerRequest *request) {
  // Redirige toutes les requêtes vers FileBrowser
  request->redirect("http://192.168.1.60:8080");
}

void SDFileServer::set_url_prefix(std::string const &prefix) { this->url_prefix_ = prefix; }
void SDFileServer::set_root_path(std::string const &path) { this->root_path_ = path; }
void SDFileServer::set_sd_mmc_card(sd_mmc_card::SdMmc *card) { this->sd_mmc_card_ = card; }
void SDFileServer::set_deletion_enabled(bool allow) { this->deletion_enabled_ = allow; }
void SDFileServer::set_download_enabled(bool allow) { this->download_enabled_ = allow; }
void SDFileServer::set_upload_enabled(bool allow) { this->upload_enabled_ = allow; }

// Méthodes inutiles désormais (elles peuvent être supprimées)
void SDFileServer::handle_get(AsyncWebServerRequest *request) const {}
void SDFileServer::write_row(AsyncResponseStream *response, sd_mmc_card::FileInfo const &info) const {}
void SDFileServer::handle_index(AsyncWebServerRequest *request, std::string const &path) const {}
void SDFileServer::handle_download(AsyncWebServerRequest *request, std::string const &path) const {}
void SDFileServer::handle_delete(AsyncWebServerRequest *request) {}

std::string SDFileServer::build_prefix() const {
  if (this->url_prefix_.length() == 0 || this->url_prefix_.at(0) != '/')
    return "/" + this->url_prefix_;
  return this->url_prefix_;
}

std::string SDFileServer::extract_path_from_url(std::string const &url) const {
  std::string prefix = this->build_prefix();
  return url.substr(prefix.size(), url.size() - prefix.size());
}

std::string SDFileServer::build_absolute_path(std::string relative_path) const {
  if (relative_path.size() == 0)
    return this->root_path_;
  std::string absolute = Path::join(this->root_path_, relative_path);
  return absolute;
}

// Classe Path : Implémentation des utilitaires pour les chemins
std::string Path::file_name(std::string const &path) {
  size_t pos = path.rfind(Path::separator);
  if (pos != std::string::npos) {
    return path.substr(pos + 1);
  }
  return "";
}

bool Path::is_absolute(std::string const &path) { return path.size() && path[0] == separator; }

bool Path::trailing_slash(std::string const &path) { return path.size() && path[path.length() - 1] == separator; }

std::string Path::join(std::string const &first, std::string const &second) {
  std::string result = first;
  if (!trailing_slash(first) && !is_absolute(second)) {
    result.push_back(separator);
  }
  if (trailing_slash(first) && is_absolute(second)) {
    result.pop_back();
  }
  result.append(second);
  return result;
}

std::string Path::remove_root_path(std::string path, std::string const &root) {
  if (!str_startswith(path, root))
    return path;
  if (path.size() == root.size() || path.size() < 2)
    return "/";
  return path.erase(0, root.size());
}

std::vector<std::string> Path::split_path(std::string path) {
  std::vector<std::string> parts;
  size_t pos = 0;
  while ((pos = path.find('/')) != std::string::npos) {
    std::string part = path.substr(0, pos);
    if (!part.empty()) {
      parts.push_back(part);
    }
    path.erase(0, pos + 1);
  }
  parts.push_back(path);
  return parts;
}

std::string Path::extension(std::string const &file) {
  size_t pos = file.find_last_of('.');
  if (pos == std::string::npos)
    return "";
  return file.substr(pos + 1);
}

std::string Path::file_type(std::string const &file) {
  static const std::map<std::string, std::string> file_types = {
      {"mp3", "Audio (MP3)"},   {"wav", "Audio (WAV)"}, {"png", "Image (PNG)"},   {"jpg", "Image (JPG)"},
      {"jpeg", "Image (JPEG)"}, {"bmp", "Image (BMP)"}, {"txt", "Text (TXT)"},    {"log", "Text (LOG)"},
      {"csv", "Text (CSV)"},    {"html", "Web (HTML)"}, {"css", "Web (CSS)"},     {"js", "Web (JS)"},
      {"json", "Data (JSON)"},  {"xml", "Data (XML)"},  {"zip", "Archive (ZIP)"}, {"gz", "Archive (GZ)"},
      {"tar", "Archive (TAR)"}, {"mp4", "Video (MP4)"}, {"avi", "Video (AVI)"},   {"webm", "Video (WEBM)"}};
  std::string ext = Path::extension(file);
  if (ext.empty())
    return "File";
  std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });
  auto it = file_types.find(ext);
  if (it != file_types.end())
    return it->second;
  return "File (" + ext + ")";
}

std::string Path::mime_type(std::string const &file) {
  static const std::map<std::string, std::string> file_types = {
      {"mp3", "audio/mpeg"},        {"wav", "audio/vnd.wav"},   {"png", "image/png"},       {"jpg", "image/jpeg"},
      {"jpeg", "image/jpeg"},       {"bmp", "image/bmp"},       {"txt", "text/plain"},      {"log", "text/plain"},
      {"csv", "text/csv"},          {"html", "text/html"},      {"css", "text/css"},        {"js", "text/javascript"},
      {"json", "application/json"}, {"xml", "application/xml"}, {"zip", "application/zip"}, {"gz", "application/gzip"},
      {"tar", "application/x-tar"}, {"mp4", "video/mp4"},       {"avi", "video/x-msvideo"}, {"webm", "video/webm"}};
  std::string ext = Path::extension(file);
  ESP_LOGD(TAG, "ext : %s", ext.c_str());
  if (!ext.empty()) {
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });
    auto it = file_types.find(ext);
    if (it != file_types.end())
      return it->second;
  }
  return "application/octet-stream";
}

}  // namespace sd_file_server
}  // namespace esphome













































































