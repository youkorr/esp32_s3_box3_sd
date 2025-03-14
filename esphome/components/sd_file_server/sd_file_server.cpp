#include "sd_file_server.h"
#include "esphome/core/log.h"
#include "esphome/components/network/util.h"
#include "esphome/core/helpers.h"
#include <map>

namespace esphome {
namespace sd_file_server {

static const char *TAG = "sd_file_server";

// Helper function to format file sizes
std::string format_size(size_t size) {
  const char *units[] = {"B", "KB", "MB", "GB"};
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

// Map file extensions to their types
std::string get_file_type(const std::string &filename) {
  static const std::map<std::string, std::string> file_types = {
      {"mp3", "Audio (MP3)"},     {"wav", "Audio (WAV)"},     {"png", "Image (PNG)"},
      {"jpg", "Image (JPG)"},     {"jpeg", "Image (JPEG)"},   {"bmp", "Image (BMP)"},
      {"txt", "Text (TXT)"},       {"log", "Text (LOG)"},       {"csv", "Text (CSV)"},
      {"html", "Web (HTML)"},     {"css", "Web (CSS)"},       {"js", "Web (JS)"},
      {"json", "Data (JSON)"},     {"xml", "Data (XML)"},     {"zip", "Archive (ZIP)"},
      {"gz", "Archive (GZ)"},     {"tar", "Archive (TAR)"}};

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
  if (first.empty())
    return second;
  if (second.empty())
    return first;

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
  if (!str_startswith(path, root))
    return path;

  path.erase(0, root.size());
  if (path.empty() || path[0] != separator) {
    path = separator + path;
  }

  return path;
}

SDFileServer::SDFileServer(web_server_base::WebServerBase *base) : base_(base) {}

void SDFileServer::setup() { this->base_->add_handler(this); }

void SDFileServer::dump_config() {
  ESP_LOGCONFIG(TAG, "SD File Server:");
  ESP_LOGCONFIG(TAG, "  Address: %s:%u", network::get_use_address().c_str(), this->base_->get_port());
  ESP_LOGCONFIG(TAG, "  Url Prefix: %s", this->url_prefix_.c_str());
  ESP_LOGCONFIG(TAG, "  Root Path: %s", this->root_path_.c_str());
  ESP_LOGCONFIG(TAG, "  Deletion Enabled: %s", YESNO(this->deletion_enabled_));
  ESP_LOGCONFIG(TAG, "  Download Enabled : %s", YESNO(this->download_enabled_));
  ESP_LOGCONFIG(TAG, "  Upload Enabled : %s", YESNO(this->upload_enabled_));
}

bool SDFileServer::canHandle(AsyncWebServerRequest *request) {
  return str_startswith(std::string(request->url().c_str()), this->build_prefix());
}

void SDFileServer::handleRequest(AsyncWebServerRequest *request) {
    ESP_LOGD(TAG, "Handling request for: %s", request->url().c_str());
    if (str_startswith(std::string(request->url().c_str()), this->build_prefix())) {
        if (request->method() == HTTP_GET) {
            ESP_LOGD(TAG, "Handling GET request");
            this->handle_get(request);
            return;
        }
        if (request->method() == HTTP_DELETE) {
            ESP_LOGD(TAG, "Handling DELETE request");
            this->handle_delete(request);
            return;
        }
        if (request->method() == HTTP_POST) {
            ESP_LOGD(TAG, "Handling POST request");

            // Check for multipart/form-data content type
            if (request->contentType().startsWith("multipart/form-data")) {
                ESP_LOGD(TAG, "Content type is multipart/form-data");
                if (request->hasParam("file", true)) {
                    ESP_LOGD(TAG, "Handling file upload POST request");
                    AsyncWebParameter* p = request->getParam("file", true);
                    if (p != nullptr) {
                        ESP_LOGD(TAG, "File parameter found");
                        String filename = p->name();
                        size_t index = 0;
                        String file_content = p->value();

                        // Allocate memory for the file data
                        uint8_t* data = new uint8_t[file_content.length() + 1];

                        // Copy the file content to the allocated memory
                        strcpy((char*)data, file_content.c_str());

                        size_t len = file_content.length();
                        bool final = true;
                        this->handleUpload(request, filename, index, data, len, final);
                        delete[] data; // Free the allocated memory
                        return;
                    } else {
                        ESP_LOGW(TAG, "file parameter is null");
                        request->send(400, "application/json", "{ \"error\": \"No file uploaded\" }");
                        return;
                    }
                } else {
                    ESP_LOGW(TAG, "No file parameter found in POST request");
                    request->send(400, "application/json", "{ \"error\": \"No file uploaded\" }");
                    return;
                }
            } else {
                ESP_LOGW(TAG, "Invalid content type. Expected multipart/form-data, got: %s", request->contentType().c_str());
                request->send(400, "application/json", "{ \"error\": \"Invalid content type. Expected multipart/form-data\" }");
                return;
            }
        }

        ESP_LOGW(TAG, "Unsupported method: %d", request->method());
        request->send(405, "application/json", "{ \"error\": \"Method not allowed\" }");
    }
}


void SDFileServer::handleUpload(AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data,
                                size_t len, bool final) {
  if (!this->upload_enabled_) {
    ESP_LOGW(TAG, "File upload is disabled");
    request->send(401, "application/json", "{ \"error\": \"file upload is disabled\" }");
    return;
  }

  std::string extracted = this->extract_path_from_url(std::string(request->url().c_str()));
  std::string path = this->build_absolute_path(extracted);

  std::string file_name_str = std::string(filename.c_str());
  std::string full_path = Path::join(path, file_name_str);

  if (index == 0) {
    if (!this->sd_mmc_card_->is_directory(path)) {
      ESP_LOGW(TAG, "Invalid upload folder: %s", path.c_str());
      request->send(400, "application/json", "{ \"error\": \"invalid upload folder\" }");
      return;
    }
    ESP_LOGD(TAG, "Uploading file %s to %s", file_name_str.c_str(), full_path.c_str());
    this->sd_mmc_card_->write_file(full_path.c_str(), data, len);
    ESP_LOGE(TAG, "Failed to write file %s", full_path.c_str());
    request->send(500, "application/json", "{ \"error\": \"failed to write file\" }");
    return;
  } else {
    ESP_LOGD(TAG, "Appending %u bytes to file %s", len, full_path.c_str());
    this->sd_mmc_card_->append_file(full_path.c_str(), data, len);
    ESP_LOGE(TAG, "Failed to append file %s", full_path.c_str());
    request->send(500, "application/json", "{ \"error\": \"failed to append file\" }");
    return;
  }

  if (final) {
    ESP_LOGI(TAG, "File %s upload complete", full_path.c_str());
    request->send(200, "application/json", "{ \"message\": \"File uploaded successfully\" }");
  }
}

void SDFileServer::set_url_prefix(std::string const &prefix) { this->url_prefix_ = prefix; }
void SDFileServer::set_root_path(std::string const &path) { this->root_path_ = path; }
void SDFileServer::set_sd_mmc_card(sd_mmc_card::SdMmc *card) { this->sd_mmc_card_ = card; }
void SDFileServer::set_deletion_enabled(bool allow) { this->deletion_enabled_ = allow; }
void SDFileServer::set_download_enabled(bool allow) { this->download_enabled_ = allow; }
void SDFileServer::set_upload_enabled(bool allow) { this->upload_enabled_ = allow; }

std::string SDFileServer::build_prefix() const { return this->url_prefix_; }

std::string SDFileServer::build_absolute_path(std::string path) const {  // Removed const&
  return Path::join(this->root_path_, path);
}

std::string SDFileServer::extract_path_from_url(std::string const &url) const {
  std::string prefix = this->build_prefix();
  if (str_startswith(url, prefix)) {
    return url.substr(prefix.length());
  }
  return url;
}

void SDFileServer::handle_get(AsyncWebServerRequest *request) const {
  std::string extracted = this->extract_path_from_url(std::string(request->url().c_str()));
  std::string path = this->build_absolute_path(extracted);

  if (!this->sd_mmc_card_->is_directory(path)) {
    handle_download(request, path);
    return;
  }

  handle_index(request, path);
}

void SDFileServer::handle_index(AsyncWebServerRequest *request, std::string const &path) const {
  AsyncResponseStream *response = request->beginResponseStream("text/html");
    response->print(F("<!DOCTYPE html><html lang=\"en\"><head><meta charset=UTF-8><meta "
                      "name=viewport content=\"width=device-width, initial-scale=1,user-scalable=no\">"
                      "<title>SD Card Files</title>"
                      "<style>"
                      "body { font-family: Arial, sans-serif; }"
                      "table { border-collapse: collapse; width: 100%; }"
                      "th, td { border: 1px solid #ddd; padding: 8px; text-align: left; }"
                      "th { background-color: #f2f2f2; }"
                      "</style>"
                      "</head><body>"
                      "<h1>SD Card Content</h1><h2>Folder "));

    response->print(path.c_str());
    response->print(F("</h2>"));
    if (this->upload_enabled_)
        response->print(F("<form method=\"POST\" enctype=\"multipart/form-data\">"
                          "<input type=\"file\" name=\"file\"><input type=\"submit\" value=\"upload\"></form>"));
    response->print(F("<a href=\"/"));
    response->print(this->url_prefix_.c_str());
    response->print(F("\">Home</a></br></br><table id=\"files\"><thead><tr><th>Name<th>Actions<tbody>"));
    auto entries = this->sd_mmc_card_->list_directory_file_info(path, 0);
    for (auto const& entry : entries)
        write_row(response, entry);

    response->print(F("</tbody></table>"
                      "<script>"
                      "function delete_file(path) {fetch(path, {method: \"DELETE\"}).then(response => {if (response.ok) { window.location.reload(); } else { alert('Failed to delete file.'); }});}"
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

void SDFileServer::handle_download(AsyncWebServerRequest *request, std::string const &path) const {
  if (!this->download_enabled_) {
    request->send(401, "application/json", "{ \"error\": \"file download is disabled\" }");
    return;
  }

  ESP_LOGD(TAG, "Serving file for download: %s", path.c_str());

  // Get file size first - using file_size() instead of get_file_size()
  size_t file_size = this->sd_mmc_card_->file_size(path);
  if (file_size == 0) {
    request->send(404, "application/json", "{ \"error\": \"file not found or empty\" }");
    return;
  }

  // Determine content type based on file extension
  std::string file_name = Path::file_name(path);
  std::string content_type = "application/octet-stream";  // Default content type

  // Get file extension and set appropriate content type
  size_t dot_pos = file_name.rfind('.');
  if (dot_pos != std::string::npos) {
    std::string ext = file_name.substr(dot_pos + 1);
    if (ext == "txt" || ext == "log" || ext == "csv") {
      content_type = "text/plain";
    } else if (ext == "html" || ext == "htm") {
      content_type = "text/html";
    } else if (ext == "css") {
      content_type = "text/css";
    } else if (ext == "js") {
      content_type = "application/javascript";
    } else if (ext == "json") {
      content_type = "application/json";
    } else if (ext == "xml") {
      content_type = "application/xml";
    } else if (ext == "png") {
      content_type = "image/png";
    } else if (ext == "jpg" || ext == "jpeg") {
      content_type = "image/jpeg";
    } else if (ext == "gif") {
      content_type = "image/gif";
    } else if (ext == "mp3") {
      content_type = "audio/mpeg";
    } else if (ext == "wav") {
      content_type = "audio/wav";
    }
  }

  // Use the file reading approach
  auto file = this->sd_mmc_card_->read_file(path);
   if (file.empty()) {
        ESP_LOGE(TAG, "Failed to read file: %s", path.c_str());
        request->send(500, "application/json", "{ \"error\": \"failed to read file\" }");
        return;
    }

#ifdef USE_ESP_IDF
  auto *response = request->beginResponse_P(200, content_type.c_str(), (const uint8_t*)file.data(), file.size());
#else
  auto *response = request->beginResponse(200, content_type.c_str(), (const char*)file.data(), file.size());
#endif

  response->addHeader("Content-Disposition", ("attachment; filename=\"" + file_name + "\"").c_str());
  request->send(response);
}

void SDFileServer::handle_delete(AsyncWebServerRequest *request) {
  if (!this->deletion_enabled_) {
    request->send(401, "application/json", "{ \"error\": \"file deletion is disabled\" }");
    return;
  }
  std::string extracted = this->extract_path_from_url(std::string(request->url().c_str()));
  std::string path = this->build_absolute_path(extracted);
  if (this->sd_mmc_card_->is_directory(path)) {
        request->send(400, "application/json", "{ \"error\": \"Cannot delete directories\" }");
        return;
  }

  if (this->sd_mmc_card_->remove_file(path.c_str())) {
        request->send(200, "application/json", "{ \"message\": \"File deleted successfully\" }");
    } else {
        request->send(500, "application/json", "{ \"error\": \"Failed to delete file\" }");
    }
}

void SDFileServer::write_row(AsyncResponseStream *response, sd_mmc_card::FileInfo const& entry) const {
    std::string uri = "/" + Path::join(this->url_prefix_, Path::remove_root_path(entry.path, this->root_path_));
    std::string file_name = Path::file_name(entry.path);

    response->print("<tr><td>");
    response->print("<a href=\"");
    response->print(uri.c_str());
    response->print("\">");
    response->print(file_name.c_str());
    response->print("</a></td><td>");
    if (!entry.is_directory) {
        if (this->download_enabled_) {
            response->print("<button onclick=\"download_file('");
            response->print(uri.c_str());
            response->print("','");
            response->print(file_name.c_str());
            response->print("')\">Download</button>");
        }
        if (this->deletion_enabled_) {
            response->print("<button onclick=\"delete_file('");
            response->print(uri.c_str());
            response->print("')\">Delete</button>");
        }
    }
    response->print("</td></tr>");
}


std::string SDFileServer::build_prefix() const { return this->url_prefix_; }

std::string SDFileServer::build_absolute_path(std::string path) const {
    return Path::join(this->root_path_, path);
}

std::string SDFileServer::extract_path_from_url(std::string const& url) const {
    std::string prefix = this->build_prefix();
    if (str_startswith(url, prefix)) {
        return url.substr(prefix.length());
    }
    return url;
}
}  // namespace sd_file_server
}  // namespace esphome

















































































