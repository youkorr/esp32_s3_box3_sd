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

  static std::vector<uint8_t> file_data;
  
  if (index == 0) {
    file_data.clear();
  }

  file_data.insert(file_data.end(), data, data + len);

  if (final) {
    String path = request->url();
    if (path.ends_with('/')) { // Correction C++20
      path += filename;
    }
    this->sd_mmc_card_->append_file(path.c_str(), file_data.data(), file_data.size());
    request->send(200);
  }
}

void SDFileServer::handle_index(AsyncWebServerRequest *request, std::string const &path) const {
  auto response = request->beginResponseStream("text/html");
  response->print("<html><body><table>");
  response->print("<tr><th>Name</th><th>Size</th><th>Type</th></tr>");

  auto files = this->sd_mmc_card_->list_directory_file_info(path.c_str(), 1);
  for (auto &file : files) {
    this->write_row(response, file);
  }

  response->print("</table></body></html>");
  request->send(response);
}

void SDFileServer::handle_download(AsyncWebServerRequest *request, std::string const &path) const {
  if (!this->download_enabled_) {
    request->send(403);
    return;
  }

  std::vector<uint8_t> file_data = this->sd_mmc_card_->read_file(path.c_str());
  if (file_data.empty()) {
    request->send(404);
    return;
  }

  auto response = request->beginResponseStream("application/octet-stream");
  response->addHeader("Content-Disposition", ("attachment; filename=\"" + path.substr(path.find_last_of('/') + 1) + "\"").c_str());
  
  // Correction pour l'API AsyncResponseStream
  response->print(std::string(
    reinterpret_cast<const char*>(file_data.data()), 
    file_data.size()
  ));
  
  request->send(response);
}

}  // namespace sd_file_server
}  // namespace esphome






