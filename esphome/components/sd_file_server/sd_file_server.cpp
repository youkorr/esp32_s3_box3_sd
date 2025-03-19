#include "sd_file_server.h"

#include "esphome/core/log.h"
#include "esphome/components/network/util.h"
#include "esphome/core/helpers.h"
#include "esphome/components/sd_mmc_card/sd_mmc_card.h"  // Include SdMmc
#include <map>
#include <sstream>
#include <iomanip>

#include <esp_err.h>
#include <esp_log.h>

namespace esphome {
namespace sd_file_server {

static const char *TAG = "sd_file_server";
static const size_t FILE_BUFFER_SIZE = 512;  // Define a buffer size

// Helper function to format file sizes
std::string format_size(size_t size) {
  const char *units[] = {"B", "KB", "MB", "GB"};
  size_t unit = 0;
  double s = static_cast<double>(size);

  while (s >= 1024 && unit < 3) {
    s /= 1024;
    unit++;
  }

  std::stringstream ss;
  ss << std::fixed << std::setprecision(2) << s << " " << units[unit];
  return ss.str();
}

// Map file extensions to their types
std::string get_file_type(const std::string &filename) {
  static const std::map<std::string, std::string> file_types = {
      {"mp3", "Audio (MP3)"},   {"wav", "Audio (WAV)"},     {"flac", "Audio (FLAC)"},
      {"gif", "Image (GIF)"},   {"png", "Image (PNG)"},     {"jpg", "Image (JPG)"},
      {"jpeg", "Image (JPEG)"}, {"bmp", "Image (BMP)"},     {"txt", "Text (TXT)"},
      {"log", "Text (LOG)"},   {"csv", "Text (CSV)"},     {"html", "Web (HTML)"},
      {"css", "Web (CSS)"},   {"js", "Web (JS)"},       {"json", "Data (JSON)"},
      {"xml", "Data (XML)"},   {"zip", "Archive (ZIP)"},   {"gz", "Archive (GZ)"},
      {"tar", "Archive (TAR)"}};

  size_t dot_pos = filename.rfind('.');
  if (dot_pos != std::string::npos) {
    std::string ext = filename.substr(dot_pos + 1);
    auto it = file_types.find(ext);
    if (it != file_types.end()) {
      return it->second;
    }
    return "File (" + ext + ")";
  }
  return "File";
}

// Implementation of Path methods
std::string Path::file_name(std::string const &path) {
  size_t pos = path.rfind(Path::separator);
  if (pos != std::string::npos) {
    return path.substr(pos + 1);
  }
  return path;
}

bool Path::is_absolute(std::string const &path) { return path.size() && path[0] == separator; }

bool Path::trailing_slash(std::string const &path) {
  return path.size() && path[path.length() - 1] == separator;
}

std::string Path::join(std::string const &first, std::string const &second) {
  if (first.empty()) return second;
  if (second.empty()) return first;

  std::string result = first;

  if (trailing_slash(result)) {
    result.pop_back();
  }

  if (is_absolute(second)) {
    result += second;
  } else {
    result += separator + second;
  }

  return result;
}

std::string Path::remove_root_path(std::string path, std::string const &root) {
  if (!str_startswith(path, root)) return path;

  path.erase(0, root.size());
  if (path.empty() || path[0] != separator) {
    path = separator + path;
  }

  return path;
}

SDFileServer::SDFileServer(web_server_base::WebServerBase *base, sd_mmc_card::SdMmc *sd_mmc)
    : base_(base), sd_mmc_(sd_mmc) {}

void SDFileServer::setup() {
  ESP_LOGCONFIG(TAG, "Setting up SD file server...");
  this->base_->add_handler(this);
}

void SDFileServer::dump_config() {
  ESP_LOGCONFIG(TAG, "SD File Server:");
  ESP_LOGCONFIG(TAG, "  Address: %s:%u", network::get_use_address().c_str(), this->base_->get_port());
  ESP_LOGCONFIG(TAG, "  Url Prefix: %s", this->url_prefix_.c_str());
}

void SDFileServer::set_url_prefix(const std::string &url_prefix) { this->url_prefix_ = url_prefix; }

void SDFileServer::set_sd_mmc(sd_mmc_card::SdMmc *sd_mmc) { this->sd_mmc_ = sd_mmc; }

bool SDFileServer::canHandle(AsyncWebServerRequest *request) { return true; }

void SDFileServer::handle_web_request(AsyncWebServerRequest *request) {
  std::string url = request->url().c_str();

  ESP_LOGD(TAG, "Handling web request for: %s", url.c_str());

  // 1. Check for URL prefix
  if (!this->url_prefix_.empty()) {
    if (!str_startswith(url, this->url_prefix_)) {
      ESP_LOGV(TAG, "URL doesn't match prefix: %s", this->url_prefix_.c_str());
      request->send(404, "text/plain", "Not found (URL prefix mismatch)");
      return;
    }
    url = url.substr(this->url_prefix_.size());  // Remove prefix
  }

  // 2. Handle static files from LittleFS
  if (LittleFS.exists(url.c_str()) && !Path::trailing_slash(url)) {
    ESP_LOGV(TAG, "Serving static file: %s", url.c_str());
    request->send(LittleFS, url.c_str(), String());  // Let AsyncWebServer handle content type
    return;
  }

  // 3. Handle special endpoints (e.g., /upload, /download)
  if (url == "/upload" && request->method() == HTTP_POST) {
    handleUpload(request);
  } else if (str_startswith(url, "/download")) {
    handleDownload(request);
  } else {
    ESP_LOGV(TAG, "File not found: %s", url.c_str());
    request->send(404, "text/plain", "Not found");
  }
}

void SDFileServer::handleUpload(AsyncWebServerRequest *request) {
  // This is a simplified upload handler.  For true chunked uploads, you would
  // need to use a custom stream class.
  if (!request->hasParam("filename", true)) {
    request->send(400, "text/plain", "Missing filename");
    return;
  }

  std::string filename = request->getParam("filename", true)->value().c_str();
  ESP_LOGI(TAG, "Start uploading file: %s", filename.c_str());

  // Accumulate the data from the request
  std::vector<uint8_t> file_content;

  // Iterate over all the arguments in the request to get the data
  int params = request->params();
  for (int i = 0; i < params; i++) {
    AsyncWebParameter *p = request->getParam(i);
    if (p->isFile()) {  // Verify that is a file
      ESP_LOGI(TAG, "File Name: %s", p->name().c_str());
      ESP_LOGI(TAG, "Size: %u bytes", p->size());
      ESP_LOGI(TAG, "Content Type: %s", p->contentType().c_str());

      if (p->size() > 0) {
        // Reserve memory for the file content.
        file_content.reserve(p->size());
        file_content.insert(file_content.end(), p->value().begin(), p->value().end());
      }
    }
  }

  // Check if the SD card component was set
  if (this->sd_mmc_ == nullptr) {
    ESP_LOGE(TAG, "SD card component not initialized!");
    request->send(500, "text/plain", "SD card component not initialized!");
    return;
  }

  // Write to file by chunks
  this->sd_mmc_->write_file_chunked(filename.c_str(), file_content.data(), file_content.size(), FILE_BUFFER_SIZE);

  // Send response
  request->send(200, "text/plain", "File uploaded successfully");
  ESP_LOGI(TAG, "Upload done: %s", filename.c_str());
}

void SDFileServer::handleDownload(AsyncWebServerRequest *request) {
  std::string url = request->url().c_str();
  size_t file_param_start = url.find("file=");

  if (file_param_start == std::string::npos) {
    request->send(400, "text/plain", "Missing 'file' parameter");
    return;
  }

  std::string filename = url.substr(file_param_start + 5);  // Get filename after "file="
  ESP_LOGI(TAG, "Start sending file: %s", filename.c_str());

  // Check if the SD card component was set
  if (this->sd_mmc_ == nullptr) {
    ESP_LOGE(TAG, "SD card component not initialized!");
    request->send(500, "text/plain", "SD card component not initialized!");
    return;
  }

  // Build the absolute path to the file
  std::string filepath = "/sdcard/" + filename;  // Assuming MOUNT_POINT is /sdcard
  FILE *file = fopen(filepath.c_str(), "rb");
  if (!file) {
    ESP_LOGE(TAG, "Failed to open file for reading: %s", filepath.c_str());
    request->send(404, "text/plain", "File not found");
    return;
  }

  // Send headers
  AsyncWebServerResponse *response = request->beginResponse(200, "application/octet-stream", false);
  response->addHeader("Content-Disposition", "attachment; filename=\"" + filename + "\"");
  request->send(response);

  // Read the file in chunks
  uint8_t *buffer = new uint8_t[FILE_BUFFER_SIZE];
  size_t bytesRead;

  while ((bytesRead = fread(buffer, 1, FILE_BUFFER_SIZE, file)) > 0) {
    response->write(buffer, bytesRead);
  }

  delete[] buffer;
  fclose(file);

  ESP_LOGI(TAG, "File sending done: %s", filename.c_str());
}

}  // namespace sd_file_server
}  // namespace esphome













































































