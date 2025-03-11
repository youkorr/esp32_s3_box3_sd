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
  auto *response = request->beginResponse("audio/mpeg");

  // Lire et envoyer les données par morceaux
  const size_t chunk_size = 4096;
  uint8_t buffer[chunk_size];
  size_t bytes_read;

  while ((bytes_read = fread(buffer, 1, chunk_size, file)) > 0) {
    response->send(buffer, bytes_read);  // Utilisation de send pour envoyer les données
  }

  // Fermer le fichier
  fclose(file);

  // Envoyer la réponse
  request->send(response);
}

void SDFileServer::handle_index(AsyncWebServerRequest *request, std::string const &path) const {
  AsyncResponseStream *response = request->beginResponseStream("text/html");
  response->print(F("<!DOCTYPE html><html lang=\"en\"><head><meta charset=UTF-8><meta "
                    "name=viewport content=\"width=device-width, initial-scale=1,user-scalable=no\">"
                    "<style>"
                    "body { font-family: Arial, sans-serif; background-color: #f4f4f4; color: #333; margin: 0; padding: 20px; }"
                    "h1 { color: #4CAF50; }"
                    "h2 { color: #555; }"
                    "table { width: 100%; border-collapse: collapse; margin-top: 20px; }"
                    "th, td { padding: 12px; text-align: left; border-bottom: 1px solid #ddd; }"
                    "th { background-color: #4CAF50; color: white; }"
                    "tr:hover { background-color: #f5f5f5; }"
                    ".download-btn { background-color: #4CAF50; color: white; border: none; padding: 8px 12px; cursor: pointer; border-radius: 4px; }"
                    ".download-btn:hover { background-color: #45a049; }"
                    "a { color: #4CAF50; text-decoration: none; }"
                    "a:hover { text-decoration: underline; }"
                    "</style>"
                    "</head><body>"
                    "<h1>SD Card Content</h1><h2>Folder "));

  response->print(path.c_str());
  response->print(F("</h2>"));
  response->print(F("<a href=\"/"));
  response->print(this->url_prefix_.c_str());
  response->print(F("\">Home</a></br></br><table id=\"files\"><thead><tr><th>Name<th>Actions<tbody>"));
  auto entries = this->sd_mmc_card_->list_directory_file_info(path, 0);
  for (auto const &entry : entries)
    write_row(response, entry);

  response->print(F("</tbody></table>"
                    "<script>"
                    "function download_file(path, filename) {"
                    "fetch(path).then(response => response.blob())"
                    ".then(blob => {"
                    "const link = document.createElement('a');"
                    "link.href = URL.createObjectURL(blob);"
                    "link.download = filename;"
                    "link.click();"
                    "}).catch(console.error);"
                    "} "
                    "</script>"
                    "</body></html>"));

  request->send(response);
}

void SDFileServer::write_row(AsyncResponseStream *response, sd_mmc_card::FileInfo const &info) const {
  std::string uri = "/" + Path::join(this->url_prefix_, Path::remove_root_path(info.path, this->root_path_));
  std::string file_name = Path::file_name(info.path);
  response->print("<tr><td>");
  if (info.is_directory) {
    response->print("<a href=\"");
    response->print(uri.c_str());
    response->print("\">");
    response->print(file_name.c_str());
    response->print("</a>");
  } else {
    response->print(file_name.c_str());
  }
  response->print("</td><td>");
  if (!info.is_directory && this->download_enabled_) {
    response->print("<button class=\"download-btn\" onClick=\"download_file('");
    response->print(uri.c_str());
    response->print("','");
    response->print(file_name.c_str());
    response->print("')\">Download</button>");
  }
  response->print("</td></tr>");
}

}  // namespace sd_file_server
}  // namespace esphome





