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
    return request->url().starts_with(this->url_prefix_.c_str());
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

  std::string path = this->root_path_ + "/" + filename.c_str();
  
  if (index == 0) {
    if (!this->sd_mmc_card_->write_file(path.c_str(), data, len, false)) {
      request->send(500);
      return;
    }
  } else {
    if (!this->sd_mmc_card_->write_file(path.c_str(), data, len, true)) {
      request->send(500);
      return;
    }
  }

  if (final) {
    request->send(200);
  }
}

void SDFileServer::handle_download(AsyncWebServerRequest *request, std::string const &path) const {
  if (!this->download_enabled_) {
    request->send(403);
    return;
  }

  std::vector<uint8_t> file_data;
  if (!this->sd_mmc_card_->read_file(path.c_str(), file_data)) {
    request->send(404);
    return;
  }

  auto response = request->beginResponse("application/octet-stream", file_data.size(), [file_data](uint8_t *buffer, size_t maxLen, size_t index) mutable -> size_t {
    size_t available = file_data.size() - index;
    size_t to_copy = (available > maxLen) ? maxLen : available;
    memcpy(buffer, file_data.data() + index, to_copy);
    return to_copy;
  });
  
  response->addHeader("Content-Disposition", "attachment; filename=\"" + path.substr(path.find_last_of('/') + 1) + "\"");
  request->send(response);
}

}  // namespace sd_file_server
}  // namespace esphome





