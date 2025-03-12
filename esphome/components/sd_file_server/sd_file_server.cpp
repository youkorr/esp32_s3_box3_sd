#include "sd_file_server.h"
#include "esphome/core/log.h"
#include "esphome/components/network/util.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace sd_file_server {

const char *const TAG = "sd_file_server";

SDFileServer::SDFileServer(web_server_base::WebServerBase *base) : base_(base) {}

void SDFileServer::setup() {
  if (this->sd_mmc_card_ == nullptr) {
    ESP_LOGE(TAG, "No SD card configured!");
    return;
  }

  if (!this->sd_mmc_card_->is_ready()) {
    ESP_LOGE(TAG, "SD card not ready!");
    return;
  }

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
  if (request->method() == HTTP_GET || request->method() == HTTP_POST || request->method() == HTTP_DELETE) {
    return request->url().startsWith(this->url_prefix_.c_str());
  }
  return false;
}

void SDFileServer::handleRequest(AsyncWebServerRequest *request) {
  if (request->method() == HTTP_GET) {
    this->handle_get(request);
  } else if (request->method() == HTTP_DELETE && this->deletion_enabled_) {
    this->handle_delete(request);
  } else {
    request->send(405);
  }
}

void SDFileServer::handleUpload(AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len, bool final) {
  if (!this->upload_enabled_) {
    request->send(403);
    return;
  }

  static File upload_file;
  if (index == 0) {
    String path = request->url();
    if (path.endsWith("/")) {
      path += filename;
    }
    upload_file = this->sd_mmc_card_->open(path.c_str(), FILE_WRITE);
    if (!upload_file) {
      request->send(500);
      return;
    }
  }

  if (upload_file.write(data, len) != len) {
    upload_file.close();
    request->send(500);
    return;
  }

  if (final) {
    upload_file.close();
    request->send(200);
  }
}

void SDFileServer::set_url_prefix(std::string const &prefix) {
  url_prefix_ = prefix;
}

void SDFileServer::set_root_path(std::string const &path) {
  root_path_ = path;
}

void SDFileServer::set_sd_mmc_card(sd_mmc_card::SdMmc *card) {
  sd_mmc_card_ = card;
}

void SDFileServer::set_deletion_enabled(bool enabled) {
  deletion_enabled_ = enabled;
}

void SDFileServer::set_download_enabled(bool enabled) {
  download_enabled_ = enabled;
}

void SDFileServer::set_upload_enabled(bool enabled) {
  upload_enabled_ = enabled;
}

std::string SDFileServer::build_prefix() const {
  std::string prefix = this->url_prefix_;
  if (!prefix.empty() && prefix.back() != '/') {
    prefix += '/';
  }
  return prefix;
}

std::string SDFileServer::extract_path_from_url(std::string const &url) const {
  std::string path = url.substr(this->url_prefix_.length());
  if (path.empty() || path[0] != '/') {
    path = "/" + path;
  }
  return path;
}

std::string SDFileServer::build_absolute_path(std::string path) const {
  if (path.empty() || path[0] != '/') {
    path = "/" + path;
  }
  return this->root_path_ + path;
}

void SDFileServer::write_row(AsyncResponseStream *response, sd_mmc_card::FileInfo const &info) const {
  response->print("<tr>");
  response->printf("<td>%s</td>", info.name.c_str());
  response->printf("<td>%d</td>", info.size);
  response->printf("<td>%s</td>", info.is_directory ? "DIR" : "FILE");
  response->print("</tr>");
}

void SDFileServer::handle_index(AsyncWebServerRequest *request, std::string const &path) const {
  auto response = request->beginResponseStream("text/html");
  response->print("<html><body><table>");
  response->print("<tr><th>Name</th><th>Size</th><th>Type</th></tr>");

  auto files = this->sd_mmc_card_->list_directory_file_info(path);
  for (auto &file : files) {
    this->write_row(response.get(), file);
  }

  response->print("</table></body></html>");
  request->send(response);
}

void SDFileServer::handle_get(AsyncWebServerRequest *request) const {
  std::string path = this->extract_path_from_url(request->url().c_str());
  std::string absolute_path = this->build_absolute_path(path);

  if (this->sd_mmc_card_->is_directory(absolute_path)) {
    this->handle_index(request, absolute_path);
  } else {
    this->handle_download(request, absolute_path);
  }
}

void SDFileServer::handle_delete(AsyncWebServerRequest *request) {
  std::string path = this->extract_path_from_url(request->url().c_str());
  std::string absolute_path = this->build_absolute_path(path);

  if (this->sd_mmc_card_->remove(absolute_path)) {
    request->send(200);
  } else {
    request->send(500);
  }
}

void SDFileServer::handle_download(AsyncWebServerRequest *request, std::string const &path) const {
  if (!this->download_enabled_) {
    request->send(403);
    return;
  }

  auto file = this->sd_mmc_card_->open(path.c_str(), FILE_READ);
  if (!file) {
    request->send(404);
    return;
  }

  auto response = request->beginResponseStream("application/octet-stream");
  response->addHeader("Content-Disposition", "attachment; filename=\"" + path.substr(path.find_last_of('/') + 1) + "\"");
  while (file.available()) {
    uint8_t buffer[512];
    size_t len = file.read(buffer, sizeof(buffer));
    response->write(buffer, len);
  }
  file.close();
  request->send(response);
}

}  // namespace sd_file_server
}  // namespace esphome





