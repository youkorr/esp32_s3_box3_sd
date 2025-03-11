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
}

void SDFileServer::dump_config() {
  ESP_LOGCONFIG(TAG, "SD File Server:");
  ESP_LOGCONFIG(TAG, "  Address: %s:%u", network::get_use_address().c_str(), this->base_->get_port());
  ESP_LOGCONFIG(TAG, "  Url Prefix: %s", this->url_prefix_.c_str());
  ESP_LOGCONFIG(TAG, "  Root Path: %s", this->root_path_.c_str());
  ESP_LOGCONFIG(TAG, "  Download Enabled : %s", TRUEFALSE(this->download_enabled_));
}

bool SDFileServer::canHandle(AsyncWebServerRequest *request) {
  return str_startswith(std::string(request->url().c_str()), this->build_prefix());
}

void SDFileServer::handleRequest(AsyncWebServerRequest *request) {
  if (str_startswith(std::string(request->url().c_str()), this->build_prefix())) {
    if (request->method() == HTTP_GET) {
      this->handle_get(request);
      return;
    }
  }
}

void SDFileServer::handle_download(AsyncWebServerRequest *request, std::string const &path) const {
  if (!this->download_enabled_) {
    request->send(401, "application/json", "{ \"error\": \"file download is disabled\" }");
    return;
  }

  FILE *file = fopen(path.c_str(), "rb");
  if (!file) {
    request->send(401, "application/json", "{ \"error\": \"failed to open file\" }");
    return;
  }

  // Créer une réponse personnalisée
  auto *response = request->beginChunkedResponse("audio/mpeg", [file](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
    size_t bytes_read = fread(buffer, 1, maxLen, file);
    if (bytes_read == 0) {
      fclose(file);  // Fermer le fichier lorsque la lecture est terminée
    }
    return bytes_read;
  });

  request->send(response);
}

}  // namespace sd_file_server
}  // namespace esphome





