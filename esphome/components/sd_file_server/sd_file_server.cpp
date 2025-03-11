#include "sd_file_server.h"
#include "esphome/core/log.h"
#include "esphome/components/network/util.h"
#include "esphome/core/helpers.h"
#include <cstdio>

namespace esphome {
namespace sd_file_server {

static const char *TAG = "sd_file_server";

SDFileServer::SDFileServer(web_server_base::WebServerBase *base) 
    : base_(base), sd_mmc_card_(nullptr), download_enabled_(false) {}

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

  std::string mime_type = "application/octet-stream";
  size_t dot_pos = path.find_last_of('.');
  if (dot_pos != std::string::npos) {
    std::string extension = path.substr(dot_pos + 1);
    if (extension == "mp3") mime_type = "audio/mpeg";
    else if (extension == "txt") mime_type = "text/plain";
    else if (extension == "jpg" || extension == "jpeg") mime_type = "image/jpeg";
    else if (extension == "png") mime_type = "image/png";
  }

  auto *response = request->beginResponseStream(mime_type.c_str());
  std::string file_name = Path::file_name(path);
  response->addHeader("Content-Disposition", ("attachment; filename=\"" + file_name + "\"").c_str());

  const size_t chunk_size = 4096;
  uint8_t buffer[chunk_size];
  size_t bytes_read;

  while ((bytes_read = fread(buffer, 1, chunk_size, file)) > 0) {
    response->print(std::string(reinterpret_cast<const char*>(buffer), bytes_read));
  }

  fclose(file);
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

// Implémentations des setters
void SDFileServer::set_sd_mmc_card(sd_mmc_card::SdMmc *card) {
  this->sd_mmc_card_ = card;
}

void SDFileServer::set_url_prefix(const std::string &prefix) {
  this->url_prefix_ = prefix;
}

void SDFileServer::set_root_path(const std::string &path) {
  this->root_path_ = path;
}

void SDFileServer::set_download_enabled(bool allow) {
  this->download_enabled_ = allow;
}

std::string SDFileServer::build_prefix() const {
  return "/" + this->url_prefix_;
}

void SDFileServer::handle_get(AsyncWebServerRequest *request) const {
  std::string url = request->url().c_str();
  if (url == this->build_prefix()) {
    this->handle_index(request, this->root_path_);
  } else {
    std::string file_path = this->build_absolute_path(url);
    this->handle_download(request, file_path);
  }
}

std::string SDFileServer::build_absolute_path(std::string relative_path) const {
  return this->root_path_ + relative_path.substr(this->url_prefix_.length());
}

// Méthodes utilitaires de Path
std::string Path::file_name(const std::string &path) {
  size_t pos = path.find_last_of(separator);
  return (pos == std::string::npos) ? path : path.substr(pos + 1);
}

std::string Path::join(const std::string &first, const std::string &second) {
  return first + separator + second;
}

std::string Path::remove_root_path(std::string path, const std::string &root) {
  if (path.find(root) == 0) {
    path.erase(0, root.length());
  }
  return path;
}

}  // namespace sd_file_server
}  // namespace esphome




