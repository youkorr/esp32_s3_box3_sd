#include "sd_file_server.h"
#include "esphome/core/log.h"
#include "esphome/components/network/util.h"
#include "esphome/core/helpers.h"
#include <map>

namespace esphome {
namespace sd_file_server {

static const char *TAG = "sd_file_server";

// Taille optimale du buffer pour ESP32-S3
static const size_t CHUNK_SIZE = 8192;

// Fonction pour formater la taille des fichiers
std::string format_size(size_t size) {
  const char* units[] = {"B", "KB", "MB", "GB"};
  size_t unit = 0;
  double s = static_cast<double>(size);
  
  while (s >= 1024 && unit < 3) {
    s /= 1024;
    unit++;
  }
  
  char buffer[32];
  snprintf(buffer, sizeof(buffer), "%.2f %s", s, units[unit]);
  return std::string(buffer);
}

// Mapping des extensions de fichiers à leurs types MIME
std::string get_file_type(const std::string &filename) {
  static const std::map<std::string, std::string> file_types = {
    {"mp3", "audio/mpeg"},
    {"wav", "audio/wav"},
    {"png", "image/png"},
    {"jpg", "image/jpeg"},
    {"jpeg", "image/jpeg"},
    {"bmp", "image/bmp"},
    {"txt", "text/plain"},
    {"log", "text/plain"},
    {"csv", "text/csv"},
    {"html", "text/html"},
    {"css", "text/css"},
    {"js", "application/javascript"},
    {"json", "application/json"},
    {"xml", "application/xml"},
    {"zip", "application/zip"},
    {"gz", "application/gzip"},
    {"tar", "application/x-tar"}
  };

  size_t dot_pos = filename.rfind('.');
  if (dot_pos != std::string::npos) {
    std::string ext = filename.substr(dot_pos + 1);
    auto it = file_types.find(ext);
    if (it != file_types.end()) {
      return it->second;
    }
    return "application/octet-stream";
  }
  return "application/octet-stream";
}

// Implémentation des méthodes Path
std::string Path::file_name(std::string const &path) {
  size_t pos = path.rfind(Path::separator);
  return (pos != std::string::npos) ? path.substr(pos + 1) : path;
}

bool Path::is_absolute(std::string const &path) {
  return !path.empty() && path[0] == separator;
}

bool Path::trailing_slash(std::string const &path) {
  return !path.empty() && path[path.length() - 1] == separator;
}

std::string Path::join(std::string const &first, std::string const &second) {
  if (first.empty()) return second;
  if (second.empty()) return first;

  std::string result = first;
  
  if (trailing_slash(result)) {
    result.pop_back();
  }

  if (!is_absolute(second)) {
    result += separator;
  }
  result += second;

  return result;
}

std::string Path::remove_root_path(std::string path, std::string const &root) {
  if (!str_startswith(path, root))
    return path;
  
  path.erase(0, root.size());
  return path.empty() || path[0] != separator ? separator + path : path;
}

SDFileServer::SDFileServer(web_server_base::WebServerBase *base) : base_(base) {}

void SDFileServer::setup() { this->base_->add_handler(this); }

void SDFileServer::dump_config() {
  ESP_LOGCONFIG(TAG, "SD File Server:");
  ESP_LOGCONFIG(TAG, "  Address: %s:%u", network::get_use_address().c_str(), this->base_->get_port());
  ESP_LOGCONFIG(TAG, "  Url Prefix: %s", this->url_prefix_.c_str());
  ESP_LOGCONFIG(TAG, "  Root Path: %s", this->root_path_.c_str());
  ESP_LOGCONFIG(TAG, "  Deletion Enabled: %s", TRUEFALSE(this->deletion_enabled_));
  ESP_LOGCONFIG(TAG, "  Download Enabled : %s", TRUEFALSE(this->download_enabled_));
  ESP_LOGCONFIG(TAG, "  Upload Enabled : %s", TRUEFALSE(this->upload_enabled_));
}

bool SDFileServer::canHandle(AsyncWebServerRequest *request) {
  return str_startswith(std::string(request->url().c_str()), this->build_prefix());
}

void SDFileServer::handleRequest(AsyncWebServerRequest *request) {
  if (request->method() == HTTP_GET) {
    this->handle_get(request);
  } else if (request->method() == HTTP_DELETE) {
    this->handle_delete(request);
  }
}

void SDFileServer::set_url_prefix(std::string const &prefix) { this->url_prefix_ = prefix; }
void SDFileServer::set_root_path(std::string const &path) { this->root_path_ = path; }
void SDFileServer::set_sd_mmc_card(sd_mmc_card::SdMmc *card) { this->sd_mmc_card_ = card; }
void SDFileServer::set_deletion_enabled(bool allow) { this->deletion_enabled_ = allow; }
void SDFileServer::set_download_enabled(bool allow) { this->download_enabled_ = allow; }
void SDFileServer::set_upload_enabled(bool allow) { this->upload_enabled_ = allow; }

}  // namespace sd_file_server
}  // namespace esphome






































