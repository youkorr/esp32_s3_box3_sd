#include "sd_file_server.h"
#include "esphome/core/log.h"
#include "esphome/components/network/util.h"
#include "esphome/core/helpers.h"
#ifdef USE_ESP32
#include <SPIFFS.h>
#endif

namespace esphome {
namespace sd_file_server {

static const char *TAG = "sd_file_server";

SDFileServer::SDFileServer(web_server_base::WebServerBase *base) : base_(base) {}

void SDFileServer::setup() { this->base_->add_handler(this); }

void SDFileServer::dump_config() {
  ESP_LOGCONFIG(TAG, "SD File Server:");
  ESP_LOGCONFIG(TAG, "  Address: %s:%u", network::get_use_address().c_str(), this->base_->get_port());
  ESP_LOGCONFIG(TAG, "  Url Prefix: %s", this->url_prefix_.c_str());
  ESP_LOGCONFIG(TAG, "  Root Path: %s", this->root_path_.c_str());
  ESP_LOGCONFIG(TAG, "  Deletion Enabled : %s", TRUEFALSE(this->deletion_enabled_));
  ESP_LOGCONFIG(TAG, "  Download Enabled : %s", TRUEFALSE(this->download_enabled_));
  ESP_LOGCONFIG(TAG, "  Upload Enabled : %s", TRUEFALSE(this->upload_enabled_));
}

bool SDFileServer::canHandle(AsyncWebServerRequest *request) {
  return str_startswith(std::string(request->url().c_str()), this->build_prefix());
}

void SDFileServer::handleRequest(AsyncWebServerRequest *request) {
  if (str_startswith(std::string(request->url().c_str()), this->build_prefix())) {
    if (request->method() == HTTP_GET) {
      std::string url = request->url().c_str();
      if (url == "/sd_card.png") {
        this->handle_image(request, "sd_card.png", "image/png");
        return;
      } else if (url == "/download.png") {
        this->handle_image(request, "download.png", "image/png");
        return;
      } else if (url == "/delete.png") {
        this->handle_image(request, "delete.png", "image/png");
        return;
      }
      this->handle_get(request);
      return;
    }
    if (request->method() == HTTP_DELETE && this->deletion_enabled_) {
      this->handle_delete(request);
      return;
    }
  }
}

void SDFileServer::handleUpload(AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data,
                                size_t len, bool final) {
  if (!this->upload_enabled_) {
    request->send(401, "application/json", "{ \"error\": \"file upload is disabled\" }");
    return;
  }

  std::string extracted = this->extract_path_from_url(std::string(request->url().c_str()));
  std::string path = this->build_absolute_path(extracted);

  if (index == 0 && !this->sd_mmc_card_->is_directory(path)) {
    auto response = request->beginResponse(401, "application/json", "{ \"error\": \"invalid upload folder\" }");
    response->addHeader("Connection", "close");
    request->send(response);
    return;
  }

  std::string file_name(filename.c_str());
  if (index == 0) {
    ESP_LOGD(TAG, "uploading file %s to %s", file_name.c_str(), path.c_str());
    this->sd_mmc_card_->write_file(Path::join(path, file_name).c_str(), data, len);
    return;
  }

  this->sd_mmc_card_->append_file(Path::join(path, file_name).c_str(), data, len);

  if (final) {
    auto response = request->beginResponse(201, "text/html", "upload success");
    response->addHeader("Connection", "close");
    request->send(response);
    return;
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

void SDFileServer::write_row(AsyncResponseStream *response, sd_mmc_card::FileInfo const &info) const {
  std::string uri = "/" + Path::join(this->url_prefix_, Path::remove_root_path(info.path, this->root_path_));
  std::string file_name = Path::file_name(info.path);
  response->print("<tr><td>");
    response->printf("<span class=\"filename-container\">");
  if (info.is_directory) {
    response->print("<a href=\"");
    response->print(uri.c_str());
    response->print("\">");
    response->print(file_name.c_str());
    response->print("</a>");
  } else {
    response->print(file_name.c_str());
  }
   response->printf("</span>");
  response->print("</td><td>");
  if (!info.is_directory && this->download_enabled_) {
       response->printf("<a href=\"%s\" class=\"icon-link download-btn\">", uri.c_str());
    response->print("<img src=\"download.png\" alt=\"Download\" style=\"width: 20px; height: 20px;\">");
    response->print("</a>");
  }
  if (!info.is_directory && this->deletion_enabled_) {
      response->printf("<a href=\"%s\" class=\"icon-link delete-btn\" onclick=\"return confirm('Are you sure?')\">", uri.c_str());
    response->print("<img src=\"delete.png\" alt=\"Delete\" style=\"width: 20px; height: 20px;\">");
          response->print("</a>");
  }
  response->print("</td></tr>");
}

void SDFileServer::handle_index(AsyncWebServerRequest *request, std::string const &path) const {
  AsyncResponseStream *response = request->beginResponseStream("text/html");
  response->print(F("<!DOCTYPE html><html lang=\"en\"><head><meta charset=UTF-8><meta "
                    "name=viewport content=\"width=device-width, initial-scale=1,user-scalable=no\">"
                    "<style>"
                    "body { font-family: Arial, sans-serif; background-color: #f0f0f0; color: #333; margin: 0; padding: 20px; }"
                    "h1 { color: #007bff; text-align: center; }"
                    "h2 { color: #666; text-align: center; }"
                    ".sd-card-image { display: block; margin: 0 auto; width: 100px; height: auto; }"
                    "table { width: 100%; border-collapse: collapse; margin-top: 20px; box-shadow: 0 0 10px rgba(0, 0, 0, 0.1); animation: fadeIn 0.5s ease-in-out; }"
                    "th, td { padding: 12px; text-align: left; border-bottom: 1px solid #ddd; }"
                    "th { background-color: #007bff; color: white; }"
                    "tr:hover { background-color: #e9ecef; transition: background-color 0.3s ease; }"
                  ".icon-link {display: inline-block; position: relative; transition: transform 0.3s ease;}"
                    ".icon-link:hover { transform: scale(1.1); }"
                    ".filename-container { display: inline-block; padding: 5px; border: 1px solid #007bff; border-radius: 5px; transition: box-shadow 0.3s ease; }"
                    ".filename-container:hover { box-shadow: 0 0 5px #007bff; }"
                    "a { color: #007bff; text-decoration: none; transition: color 0.3s ease; }"
                    "a:hover { text-decoration: underline; color: #0056b3; }"
                    "@keyframes fadeIn { from { opacity: 0; } to { opacity: 1; } }"
                    "</style>"
                    "</head><body>"
                    "<img src=\"sd_card.png\" alt=\"SD Card\" class=\"sd-card-image\">"
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
                    "}"
                     "function delete_file(path) {"
                    "if (confirm('Are you sure you want to delete this file?')) {"
                    "fetch(path, { method: 'DELETE' })"
                    ".then(response => {"
                    "if (response.ok) {"
                    "alert('File deleted successfully');"
                    "window.location.reload();"
                    "} else {"
                    "alert('Failed to delete file');"
                    "}"
                    "}).catch(error => {"
                    "alert('Error deleting file: ' + error);"
                    "});"
                    "}"
                    "}"
                    "</script>"
                    "</body></html>"));

  request->send(response);
}

void SDFileServer::handle_download(AsyncWebServerRequest *request, std::string const &path) const {
  if (!this->download_enabled_) {
    request->send(401, "application/json", "{ \"error\": \"file download is disabled\" }");
    return;
  }

  auto file = this->sd_mmc_card_->read_file(path);
  if (file.size() == 0) {
    request->send(401, "application/json", "{ \"error\": \"failed to read file\" }");
    return;
  }
#ifdef USE_ESP_IDF
   AsyncWebServerResponse *response = request->beginResponse("application/octet-stream", file.size(), 
    [file](uint8_t *buffer, size_t len, size_t index) -> size_t {
      if (index >= file.size()) {
        return 0;
      }
      size_t bytes_to_copy = std::min(len, file.size() - index);
      memcpy(buffer, file.data() + index, bytes_to_copy);
      return bytes_to_copy;
    });
  request->send(response);
#else
    AsyncWebServerResponse *response = request->beginResponseStream("application/octet-stream");
    response->write(file.data(), file.size());
    request->send(response);
#endif


}

void SDFileServer::handle_delete(AsyncWebServerRequest *request) {
   if (!this->deletion_enabled_) {
    request->send(401, "application/json", "{ \"error\": \"deletion is disabled\" }");
    return;
  }
  std::string extracted = this->extract_path_from_url(std::string(request->url().c_str()));
  std::string path = this->build_absolute_path(extracted);
  if (!this->sd_mmc_card_->delete_file(path)) {
    request->send(500, "application/json", "{ \"error\": \"deleting file failed\" }");
    return;
  }
  request->send(200, "application/json", "{ \"message\": \"file deleted\" }");
}

void SDFileServer::handle_image(AsyncWebServerRequest *request, const std::string& filename, const std::string& contentType) const {
    if (!file_exists(filename)) {
        request->send(404, "text/plain", "File not found");
        return;
    }

    auto file = this->sd_mmc_card_->read_file(filename);
    if (file.empty()) {
        request->send(500, "text/plain", "Failed to read file");
        return;
    }
#ifdef USE_ESP_IDF
  AsyncWebServerResponse *response = request->beginResponse(contentType.c_str(), file.size(), 
    [file](uint8_t *buffer, size_t len, size_t index) -> size_t {
      if (index >= file.size()) {
        return 0;
      }
      size_t bytes_to_copy = std::min(len, file.size() - index);
      memcpy(buffer, file.data() + index, bytes_to_copy);
      return bytes_to_copy;
    });
  request->send(response);
#else
    AsyncWebServerResponse *response = request->beginResponseStream(contentType.c_str());
    response->write(file.data(), file.size());
    request->send(response);
#endif
}

bool SDFileServer::file_exists(const std::string& filename) const {
#ifdef USE_ESP32
  fs::SPIFFSFS& spiffs = SPIFFS;
  return spiffs.exists(filename.c_str());
#else
  // Implement your file exists logic here
  // For example, assuming you're using SD_MMC library
  // You might use SD_MMC.exists(filename.c_str());
  // Remember to include SD_MMC.h
  return true;
#endif
}

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

}  // namespace sd_file_server
}  // namespace esphome



















