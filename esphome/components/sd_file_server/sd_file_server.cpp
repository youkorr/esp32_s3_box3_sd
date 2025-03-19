#include "sd_file_server.h"
#include "esphome/core/log.h"
#include "esphome/components/network/util.h"
#include "esphome/core/helpers.h"

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
  ESP_LOGCONFIG(TAG, "  Deletion Enabled: %s", TRUEFALSE(this->deletion_enabled_));
  ESP_LOGCONFIG(TAG, "  Download Enabled : %s", TRUEFALSE(this->download_enabled_));
  ESP_LOGCONFIG(TAG, "  Upload Enabled : %s", TRUEFALSE(this->upload_enabled_));
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
    if (request->method() == HTTP_DELETE) {
      this->handle_delete(request);
      return;
    }
  }
}

void SDFileServer::handleUpload(AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data,
                                size_t len, bool final) {
  if (!this->upload_enabled_) {
    request->send(403, "text/plain", "Upload not allowed");
    return;
  }

  if (this->sd_mmc_card_ == nullptr) {
    request->send(500, "text/plain", "SD card not initialized");
    return;
  }

  static std::string upload_path;
  static FILE *upload_file = nullptr;
  static size_t total_size = 0;
  static bool use_chunked_mode = false;
  static std::vector<uint8_t> current_chunk;

  // Première partie du fichier
  if (index == 0) {
    std::string path = extract_path_from_url(request->url().c_str());
    path = build_absolute_path(path);

    std::string dir = path.substr(0, path.find_last_of('/'));
    if (!this->sd_mmc_card_->is_directory(dir.c_str())) {
      ESP_LOGI(TAG, "Creating directory: %s", dir.c_str());
      if (!this->sd_mmc_card_->create_directory(dir.c_str())) {
        request->send(500, "text/plain", "Failed to create directory");
        return;
      }
    }

    upload_path = path;
    total_size = 0;
    use_chunked_mode = (request->contentLength() > 40 * 1024);

    if (!use_chunked_mode) {
      // upload_file = this->sd_mmc_card_->append_file(path.c_str(), "wb"); //OLD CODE
      // NEW CODE
      upload_file = fopen(path.c_str(), "wb");
      if (upload_file == nullptr) {
        request->send(500, "text/plain", "Failed to create file");
        return;
      }
    }
  }

  // Traitement des données
  if (!use_chunked_mode) {
    if (upload_file != nullptr) {
      size_t bytes_written = fwrite(data, 1, len, upload_file);
      if (bytes_written != len) {
        ESP_LOGE(TAG, "Write error");
        fclose(upload_file);
        upload_file = nullptr;
        request->send(500, "text/plain", "Write error");
        return;
      }
      total_size += bytes_written;
    }
  } else {
    current_chunk.insert(current_chunk.end(), data, data + len);
    total_size += len;

    if (final || current_chunk.size() > 32 * 1024) {
      bool success = this->write_file_chunked(upload_path, [&](uint8_t* buffer, size_t max_len) -> size_t {
        size_t bytes_to_copy = std::min(current_chunk.size(), max_len);
        memcpy(buffer, current_chunk.data(), bytes_to_copy);
        current_chunk.erase(current_chunk.begin(), current_chunk.begin() + bytes_to_copy);
        return bytes_to_copy;
      }, 4096);

      if (!success) {
        request->send(500, "text/plain", "Write error in chunked mode");
        return;
      }
    }
  }

  // Partie finale du fichier
  if (final) {
    if (!use_chunked_mode && upload_file != nullptr) {
      fclose(upload_file);
      upload_file = nullptr;
    }

    ESP_LOGI(TAG, "Upload complete: %s (%d bytes)", upload_path.c_str(), total_size);
    std::string redir = Path::join(url_prefix_, Path::remove_root_path(upload_path, root_path_));
    redir = redir.substr(0, redir.find_last_of('/') + 1);
    request->redirect(redir.c_str());
  }
}

bool SDFileServer::write_file_chunked(const std::string &path, const std::function<size_t(uint8_t*, size_t)> &data_provider, size_t chunk_size) {
  FILE *file = fopen(path.c_str(), "ab");
  if (!file) {
    ESP_LOGE(TAG, "Failed to open file for writing: %s", path.c_str());
    return false;
  }

  uint8_t buffer[chunk_size];
  size_t bytes_read;
  while ((bytes_read = data_provider(buffer, chunk_size)) > 0) {
    size_t bytes_written = fwrite(buffer, 1, bytes_read, file);
    if (bytes_written != bytes_read) {
      ESP_LOGE(TAG, "Write error: %s", path.c_str());
      fclose(file);
      return false;
    }
  }

  fclose(file);
  return true;
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

  if (info.is_directory) {
    response->print("Folder");
  } else {
    response->print("<span class=\"file-type\">");
    response->print(Path::file_type(file_name).c_str());
    response->print("</span>");
  }
  response->print("</td><td>");
  if (!info.is_directory) {
    response->print(sd_mmc_card::format_size(info.size).c_str());
  }
  response->print("</td><td><div class=\"file-actions\">");
  if (!info.is_directory) {
    if (this->download_enabled_) {
      response->print("<button onClick=\"download_file('");
      response->print(uri.c_str());
      response->print("','");
      response->print(file_name.c_str());
      response->print("')\">Download</button>");
    }
    if (this->deletion_enabled_) {
      response->print("<button onClick=\"delete_file('");
      response->print(uri.c_str());
      response->print("')\">Delete</button>");
    }
  }
  response->print("<div></td></tr>");
}

void SDFileServer::handle_index(AsyncWebServerRequest *request, std::string const &path) const {
  AsyncResponseStream *response = request->beginResponseStream("text/html");
  response->print(F(R"(
  <!DOCTYPE html>
  <html lang=\"en\">
  <head>
    <meta charset=UTF-8>
    <meta name=viewport content=\"width=device-width, initial-scale=1,user-scalable=no\">
    <title>SD Card Files</title>
    <style>
    body {
      font-family: 'Segoe UI', system-ui, sans-serif;
      margin: 0;
      padding: 2rem;
      background: #f5f5f7;
      color: #1d1d1f;
    }
    h1 {
      color: #0066cc;
      margin-bottom: 1.5rem;
      display: flex;
      align-items: center;
      gap: 1rem;
    }
    .container {
      max-width: 1200px;
      margin: 0 auto;
      background: white;
      border-radius: 12px;
      box-shadow: 0 4px 12px rgba(0, 0, 0, 0.1);
      padding: 2rem;
    }
    table {
      width: 100%;
      border-collapse: collapse;
      margin-top: 1.5rem;
    }
    th, td {
      padding: 12px;
      text-align: left;
      border-bottom: 1px solid #e0e0e0;
    }
    th {
      background: #f8f9fa;
      font-weight: 500;
    }
    .file-actions {
      display: flex;
      gap: 8px;
    }
    button {
      padding: 6px 12px;
      border: none;
      border-radius: 6px;
      background: #0066cc;
      color: white;
      cursor: pointer;
      transition: background 0.2s;
    }
    button:hover {
      background: #0052a3;
    }
    .upload-form {
      margin-bottom: 2rem;
      padding: 1rem;
      background: #f8f9fa;
      border-radius: 8px;
    }
    .upload-form input[type="file"] {
      margin-right: 1rem;
    }
    .breadcrumb {
      margin-bottom: 1.5rem;
      font-size: 0.9rem;
      color: #666;
    }
    .breadcrumb a {
      color: #0066cc;
      text-decoration: none;
    }
    .breadcrumb a:hover {
      text-decoration: underline;
    }
    .breadcrumb a:not(:last-child)::after {
      display: inline-block;
      margin: 0 .25rem;
      content: ">";
    }
    .folder {
      color: #0066cc;
      font-weight: 500;
    }
    .file-type {
      color: #666;
      font-size: 0.9rem;
    }
    .folder-icon {
      width: 20px;
      height: 20px;
      margin-right: 8px;
      vertical-align: middle;
    }
    .header-actions {
      display: flex;
      align-items: center;
      gap: 1rem;
    }
    .header-actions button {
      background: #4CAF50;
    }
    .header-actions button:hover {
      background: #45a049;
    }
  </style>
  </head>
  <body>
  <div class="container">
    <div class="header-actions">
      <h1>SD Card Files</h1>
      <button onclick="window.location.href='/'">Go to web server</button>
    </div>
    <div class="breadcrumb">
      <a href="/">Home</a>)"));

  std::string current_path = "/";
  std::string relative_path = Path::join(this->url_prefix_, Path::remove_root_path(path, this->root_path_));
  std::vector<std::string> parts = Path::split_path(relative_path);
  for (std::string const &part : parts) {
    if (!part.empty()) {
      current_path = Path::join(current_path, part);
      response->print("<a href=\"");
      response->print(current_path.c_str());
      response->print("\">");
      response->print(part.c_str());
      response->print("</a>");
    }
  }
  response->print(F("</div>"));

  if (this->upload_enabled_)
    response->print(F("<div class=\"upload-form\"><form method=\"POST\" enctype=\"multipart/form-data\">"
                      "<input type=\"file\" name=\"file\"><input type=\"submit\" value=\"upload\"></form></div>"));

  response->print(F("<table><thead><tr>"
                    "<th>Name</th>"
                    "<th>Type</th>"
                    "<th>Size</th>"
                    "<th>Actions</th>"
                    "</tr></thead><tbody>"));

  auto entries = this->sd_mmc_card_->list_directory_file_info(path, 0);
  for (auto const &entry : entries)
    write_row(response, entry);

  response->print(F("</tbody></table>"
                    "<script>"
                    "function delete_file(path) {fetch(path, {method: \"DELETE\"});}"
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

  auto file = this->sd_mmc_card_->read_file(path);
  if (file.size() == 0) {
    request->send(401, "application/json", "{ \"error\": \"failed to read file\" }");
    return;
  }
#ifdef USE_ESP_IDF
  auto *response = request->beginResponse_P(200, Path::mime_type(path).c_str(), file.data(), file.size());
#else
  auto *response = request->beginResponseStream(Path::mime_type(path).c_str(), file.size());
  response->write(file.data(), file.size());
#endif

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
    request->send(401, "application/json", "{ \"error\": \"cannot delete a directory\" }");
    return;
  }
  if (this->sd_mmc_card_->delete_file(path)) {
    request->send(204, "application/json", "{}");
    return;
  }
  request->send(401, "application/json", "{ \"error\": \"failed to delete file\" }");
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

std::vector<std::string> Path::split_path(std::string path) {
  std::vector<std::string> parts;
  size_t pos = 0;
  while ((pos = path.find('/')) != std::string::npos) {
    std::string part = path.substr(0, pos);
    parts.push_back(part);
    path.erase(0, pos + 1);
  }
  parts.push_back(path);
  return parts;
}

std::string Path::extension(std::string const &);

std::string Path::file_type(std::string const &);

std::string Path::mime_type(std::string const &);
};

}  // namespace sd_file_server
}  // namespace esphome












































































