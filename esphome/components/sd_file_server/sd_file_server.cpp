#include "sd_file_server.h"
#include "esphome/core/log.h"
#include "esphome/components/network/util.h"
#include "esphome/core/helpers.h"
#include <cstdio>        // Pour FILE, fopen, fclose, etc.
#include <algorithm>     // Pour std::min
#include <cstring>       // Pour strlen, strcpy
#include <string>        // Pour std::string
#include <Arduino.h>      // Pour les types Arduino si nécessaires
#include <vector>

namespace esphome {
namespace sd_file_server {

static const char *TAG = "sd_file_server";

SDFileServer::SDFileServer(web_server_base::WebServerBase *base) : base_(base) {}

void SDFileServer::setup() {
  this->base_->add_handler(this);

  // Nouvelle implémentation de la gestion des uploads
  this->base_->get_server()->on(
      "/upload", HTTP_POST,
      [this](AsyncWebServerRequest *request) {
        // Gestion de la fin de l'upload
        request->send(200);
      },
      [this](AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len, bool final) {
        std::string filename_str(filename.c_str());
        this->handleUpload(request, filename_str, index, data, len, final);
      });
}

void SDFileServer::dump_config() {
  ESP_LOGCONFIG(TAG, "SD File Server:");
  ESP_LOGCONFIG(TAG, "  Address: %s:%u", network::get_use_address().c_str(), this->base_->get_port());
  ESP_LOGCONFIG(TAG, "  Url Prefix: %s", this->url_prefix_.c_str());
  ESP_LOGCONFIG(TAG, "  Root Path: %s", this->root_path_.c_str());
  ESP_LOGCONFIG(TAG, "  Deletion Enabled: %s", TRUEFALSE(this->deletion_enabled_));
  ESP_LOGCONFIG(TAG, "  Download Enabled: %s", TRUEFALSE(this->download_enabled_));
  ESP_LOGCONFIG(TAG, "  Upload Enabled: %s", TRUEFALSE(this->upload_enabled_));
}

bool SDFileServer::canHandle(AsyncWebServerRequest *request) {
  return str_startswith(std::string(request->url().c_str()), this->build_prefix());
}

void SDFileServer::handleRequest(AsyncWebServerRequest *request) {
  if (str_startswith(std::string(request->url().c_str()), this->build_prefix())) {
    std::string extracted = this->extract_path_from_url(std::string(request->url().c_str()));
    std::string path = this->build_absolute_path(extracted);
    ESP_LOGD(TAG, "Handling request: %s, Method: %d, Path: %s", request->url().c_str(), request->method(),
             path.c_str());

    if (request->method() == HTTP_GET) {
      this->handle_get(request);
      return;
    }

    if (request->method() == HTTP_POST && this->upload_enabled_) {
      request->send(200);
      return;
    }

    if (request->method() == HTTP_DELETE && this->deletion_enabled_) {
      this->handle_delete(request, path);  // Pass path to handle_delete
      return;
    }

    request->send(405, "text/plain", "Method Not Allowed");
  }
}

void SDFileServer::handleUpload(AsyncWebServerRequest *request, const std::string &filename, size_t index, uint8_t *data,
                                size_t len, bool final) {
  if (!this->upload_enabled_) {
    request->send(401, "application/json", "{ \"error\": \"file upload is disabled\" }");
    return;
  }

  std::string extracted = this->extract_path_from_url(std::string(request->url().c_str()));
  std::string path = this->build_absolute_path(extracted);
  std::string file_name = filename;
  std::string full_path = Path::join(path, file_name);

  ESP_LOGD(TAG, "Upload request: %s, File: %s, Index: %u, Length: %u, Final: %d",
           path.c_str(), file_name.c_str(), index, len, final);

  if (index == 0 && !this->sd_mmc_card_->is_directory(path)) {
    ESP_LOGE(TAG, "Upload failed: destination %s is not a directory", path.c_str());
    auto response = request->beginResponse(401, "application/json", "{ \"error\": \"invalid upload folder\" }");
    response->addHeader("Connection", "close");
    request->send(response);
    return;
  }

  if (index == 0) {
    ESP_LOGD(TAG, "Starting upload of file %s to %s", file_name.c_str(), path.c_str());
    this->sd_mmc_card_->delete_file(full_path);
    this->sd_mmc_card_->write_file(full_path.c_str(), data, len);
    return;
  }

  this->sd_mmc_card_->append_file(full_path.c_str(), data, len);

  if (final) {
    ESP_LOGD(TAG, "Completed upload of file %s, total size: %u", file_name.c_str(), index + len);
    auto response = request->beginResponse(201, "application/json", "{ \"message\": \"upload success\" }");
    response->addHeader("Connection", "close");
    request->send(response);
  }
}

void SDFileServer::set_url_prefix(std::string const &prefix) { this->url_prefix_ = prefix; }
void SDFileServer::set_root_path(std::string const &path) { this->root_path_ = path; }
void SDFileServer::set_sd_mmc_card(sd_mmc_card::SdMmc *card) { this->sd_mmc_card_ = card; }
void SDFileServer::set_deletion_enabled(bool allow) { this->deletion_enabled_ = allow; }
void SDFileServer::set_download_enabled(bool allow) { this->download_enabled_ = allow; }
void SDFileServer::set_upload_enabled(bool allow) { this->upload_enabled_ = allow; }

void SDFileServer::handle_get(AsyncWebServerRequest *request) const {
  std::string extracted = this->extract_path_from_url(std::string(request->url().c_str()));
  std::string path = this->build_absolute_path(extracted);

  if (!this->sd_mmc_card_->is_directory(path)) {
    handle_download(request, path);
    return;
  }

  handle_index(request, path);
}

std::string format_file_size(size_t size) {
  const char *units[] = {"B", "KB", "MB", "GB"};
  size_t unit_index = 0;
  double size_d = static_cast<double>(size);  // Cast to double

  while (size_d >= 1024 && unit_index < 3) {
    size_d /= 1024;
    unit_index++;
  }

  char buffer[32];
  snprintf(buffer, sizeof(buffer), "%.2f%s", size_d, units[unit_index]);  // Fixed: Removed extra space
  return std::string(buffer);
}

std::string get_file_extension(const std::string &filename) {
  size_t dot_pos = filename.rfind('.');
  if (dot_pos == std::string::npos) return "File";
  std::string ext = filename.substr(dot_pos + 1);
  std::transform(ext.begin(), ext.end(), ext.begin(), ::toupper);
  return ext + " File";
}

void SDFileServer::write_row(AsyncResponseStream *response, sd_mmc_card::FileInfo const &info) const {
  std::string uri = "/" + Path::join(this->url_prefix_, Path::remove_root_path(info.path, this->root_path_));
  std::string file_name = Path::file_name(info.path);
  std::string file_size = info.is_directory ? "-" : format_file_size(info.size);
  std::string file_type = info.is_directory ? "Folder" : get_file_extension(file_name);

  response->print("<tr><td>");
  if (info.is_directory) {
    response->print("<a href=\"");
    response->print(uri.c_str());
    response->print("\" class=\"folder-link\">");
    response->print(file_name.c_str());
    response->print("</a>");
  } else {
    response->print(file_name.c_str());
  }
  response->print("</td><td>");
  response->print(file_type.c_str());
  response->print("</td><td>");
  response->print(file_size.c_str());
  response->print("</td><td>");

  if (!info.is_directory) {
    if (this->download_enabled_) {
      response->print("<a href=\"/download?path=");
      response->print(uri.c_str());
      response->print("\" download=\"");
      response->print(file_name.c_str());
      response->print("\">Download</a>&nbsp;");
    }
    if (this->deletion_enabled_) {
      response->print("<a href=\"/delete?path=");
      response->print(uri.c_str());
      response->print("\">Delete</a>");
    }
  }
  response->print("</td></tr>");
}

void SDFileServer::handle_index(AsyncWebServerRequest *request, std::string const &path) const {
  AsyncResponseStream *response = request->beginResponseStream("text/html");
  response->print(F("<!DOCTYPE html><html><head><title>SD Card Content</title><style>body{font-family:sans-serif;}"));
  response->print(F("table{border-collapse: collapse; width: 100%;} th, td {border: 1px solid #ddd; padding: 8px;}"));
  response->print(F("th {padding-top: 12px; padding-bottom: 12px; text-align: left; background-color: #4CAF50; color: white;}"));
  response->print(F(".folder-link {text-decoration:none; font-weight:bold;}"));
  response->print(F("</style></head><body><h1>SD Card Content</h1><h2>Folder "));
  response->print(path.c_str());
  response->print(F("</h2>"));

  if (this->upload_enabled_) {
    response->print(F("<form method='POST' action='/upload' enctype='multipart/form-data'><input type='file' name='file'>"));
    response->print(F("<input type='submit' value='Upload'></form>"));
  }

  response->print(F("<p><a href=\""));
  response->print(this->url_prefix_.c_str());
  response->print(F("\" class=\"home-link\">← Back to Home</a></p>"));
  response->print(F("<table><thead><tr><th>Name</th><th>Type</th><th>Size</th><th>Actions</th></tr></thead><tbody>"));

  auto entries = this->sd_mmc_card_->list_directory_file_info(path, 0);
  for (auto const &entry : entries) {
    write_row(response, entry);
  }

  response->print(F("</tbody></table></body></html>"));
  request->send(response);
}

void SDFileServer::handle_delete(AsyncWebServerRequest *request, std::string const &path) {
    ESP_LOGD(TAG, "Handling delete request for path: %s", path.c_str());
    if (!this->deletion_enabled_) {
        request->send(403, "text/plain", "Deletion is disabled");
        return;
    }

    if (this->sd_mmc_card_->delete_file(path)) {
        ESP_LOGI(TAG, "File deleted successfully: %s", path.c_str());
        request->send(200, "text/plain", "File deleted");
    } else {
        ESP_LOGE(TAG, "Failed to delete file: %s", path.c_str());
        request->send(500, "text/plain", "Failed to delete file");
    }
}

void SDFileServer::handle_download(AsyncWebServerRequest *request, std::string const &path) const {
    ESP_LOGD(TAG, "Handling download request for path: %s", path.c_str());
    if (!this->download_enabled_) {
        request->send(403, "text/plain", "Download is disabled");
        return;
    }

    FILE *f = fopen(path.c_str(), "rb");
    if (f == nullptr) {
        ESP_LOGE(TAG, "Failed to open file for reading: %s", path.c_str());
        request->send(404, "text/plain", "File not found");
        return;
    }

    fseek(f, 0, SEEK_END);
    size_t file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    auto *response = request->beginResponse(200, "application/octet-stream",
        [f, file_size](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
            if (index >= file_size) {
                fclose(f);
                return 0;
            }
            fseek(f, index, SEEK_SET);
            size_t bytes_read = fread(buffer, 1, maxLen, f);
            return bytes_read;
    });
    response->setContentLength(file_size);
    request->send(response);
}

// Implementations for Path methods (example)
std::string Path::file_name(std::string const &path) {
  size_t pos = path.rfind(separator);
  if (pos == std::string::npos) {
    return path;
  }
  return path.substr(pos + 1);
}

bool Path::is_absolute(std::string const &path) { return !path.empty() && path[0] == separator; }

bool Path::trailing_slash(std::string const &path) { return !path.empty() && path[path.length() - 1] == separator; }

std::string Path::join(std::string const &base, std::string const &addition) {
  if (addition.empty()) return base;
  if (is_absolute(addition)) return addition;
  std::string result = base;
  if (!trailing_slash(base) && !addition.empty() && addition[0] != separator) {
    result += separator;
  }
  result += addition;
  return result;
}

std::string Path::remove_root_path(std::string path, std::string const &root) {
    if (str_startswith(path, root)) {
        return path.substr(root.length());
    }
    return path;
}

std::string SDFileServer::build_prefix() const {
    return this->url_prefix_;
}

std::string SDFileServer::extract_path_from_url(const std::string &url) const {
    std::string prefix = this->build_prefix();
    if (str_startswith(url, prefix)) {
        std::string path = url.substr(prefix.length());
        return path;
    }
    return "";
}

std::string SDFileServer::build_absolute_path(std::string path) const {
    if (Path::is_absolute(path)) {
        return Path::join(this->root_path_, path);
    } else {
        return Path::join(this->root_path_, path);
    }
}

}  // namespace sd_file_server
}  // namespace esphome






























