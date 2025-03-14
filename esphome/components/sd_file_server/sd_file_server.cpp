#include "sd_file_server.h"
#include "esphome/core/log.h"
#include "esphome/components/network/util.h"
#include "esphome/core/helpers.h"
#include <map>

namespace esphome {
namespace sd_file_server {

static const char *TAG = "sd_file_server";

std::string format_size(size_t size) {
  const char* units[] = {"B", "KB", "MB", "GB"};
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

std::string get_file_type(const std::string &filename) {
  static const std::map<std::string, std::string> file_types = {
    {"mp3", "Audio (MP3)"}, {"wav", "Audio (WAV)"}, {"png", "Image (PNG)"},
    {"jpg", "Image (JPG)"}, {"jpeg", "Image (JPEG)"}, {"bmp", "Image (BMP)"},
    {"txt", "Text (TXT)"}, {"log", "Text (LOG)"}, {"csv", "Text (CSV)"},
    {"html", "Web (HTML)"}, {"css", "Web (CSS)"}, {"js", "Web (JS)"},
    {"json", "Data (JSON)"}, {"xml", "Data (XML)"}, {"zip", "Archive (ZIP)"},
    {"gz", "Archive (GZ)"}, {"tar", "Archive (TAR)"}
  };
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

std::string Path::file_name(std::string const &path) {
  size_t pos = path.rfind(Path::separator);
  if (pos != std::string::npos) {
    return path.substr(pos + 1);
  }
  return path;
}

bool Path::is_absolute(std::string const &path) {
  return path.size() && path[0] == separator;
}

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
  if (!str_startswith(path, root))
    return path;
  path.erase(0, root.size());
  if (path.empty() || path[0] != separator) {
    path = separator + path;
  }
  return path;
}

SDFileServer::SDFileServer(web_server_base::WebServerBase *base) : base_(base) {}

void SDFileServer::setup() {
  this->base_->add_handler(this);

  // Ajouter un gestionnaire personnalisé pour les uploads
  class UploadHandler : public AsyncWebHandler {
   public:
    UploadHandler(SDFileServer *server) : server_(server) {}

    bool canHandle(AsyncWebServerRequest *request) override {
      return request->method() == HTTP_POST && request->hasParam("file");
    }

    void handleRequest(AsyncWebServerRequest *request) override {
      auto *file_param = request->getParam("file");
      if (!file_param || !file_param->isPost()) {
        request->send(400, "text/plain", "Missing or invalid file parameter");
        return;
      }
      std::string filename = file_param->value().c_str();
      server_->handleUpload(request, filename.c_str(), 0, nullptr, 0, false);
    }

   private:
    SDFileServer *server_;
  };

  this->base_->add_handler(new UploadHandler(this));
}

void SDFileServer::dump_config() {
  ESP_LOGCONFIG(TAG, "SD File Server:");
  ESP_LOGCONFIG(TAG, "  Address: %s:%u", network::get_use_address().c_str(), this->base_->get_port());
  ESP_LOGCONFIG(TAG, "  Url Prefix: %s", this->url_prefix_.c_str());
  ESP_LOGCONFIG(TAG, "  Root Path: %s", this->root_path_.c_str());
  ESP_LOGCONFIG(TAG, "  Deletion Enabled: %s", TRUEFALSE(this->deletion_enabled_));
  ESP_LOGCONFIG(TAG, "  Download Enabled: %s", TRUEFALSE(this->download_enabled_));
  ESP_LOGCONFIG(TAG, "  Upload Enabled: %s", TRUEFALSE(this->upload_enabled_));
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
    if (request->method() == HTTP_POST) {
      if (!request->hasParam("file")) {
        request->send(200, "text/plain", "Upload en attente de fichier");
      }
      return;
    }
  }
  request->send(404);
}

void SDFileServer::handleUpload(AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data,
                                size_t len, bool final) {
  if (!this->upload_enabled_) {
    ESP_LOGE(TAG, "Upload rejeté : uploads désactivés");
    request->send(403, "application/json", "{ \"error\": \"L'upload de fichiers est désactivé\" }");
    return;
  }

  std::string extracted = this->extract_path_from_url(std::string(request->url().c_str()));
  std::string path = this->build_absolute_path(extracted);
  std::string file_name(filename.c_str());

  if (index == 0) {
    if (!this->sd_mmc_card_->is_directory(path)) {
      ESP_LOGE(TAG, "Upload rejeté : %s n'est pas un répertoire", path.c_str());
      request->send(400, "application/json", "{ \"error\": \"Répertoire de destination invalide\" }");
      return;
    }
    ESP_LOGI(TAG, "Début d'upload du fichier %s vers %s", file_name.c_str(), path.c_str());
    this->sd_mmc_card_->write_file(Path::join(path, file_name).c_str(), data, len);
  } else {
    this->sd_mmc_card_->append_file(Path::join(path, file_name).c_str(), data, len);
  }

  if (final) {
    ESP_LOGI(TAG, "Upload terminé avec succès : %s", file_name.c_str());
    auto response = request->beginResponse(303, "text/plain", "Upload réussi");
    response->addHeader("Location", request->url().c_str());
    request->send(response);
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

void SDFileServer::handle_index(AsyncWebServerRequest *request, std::string const &path) const {
  AsyncResponseStream *response = request->beginResponseStream("text/html");
  response->print(F(R"(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
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
      box-shadow: 0 4px 12px rgba(0,0,0,0.1);
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
      <button onclick="window.location.href='/'">Accéder à Web Server</button>
    </div>
    <div class="breadcrumb">
      <a href="/">Home</a> > <a href="/files">files</a> > )"));

  std::string current_path = "/files/";
  std::string relative_path = Path::remove_root_path(path, this->root_path_);
  std::vector<std::string> parts;
  size_t pos = 0;
  while ((pos = relative_path.find('/')) != std::string::npos) {
    std::string part = relative_path.substr(0, pos);
    if (!part.empty()) {
      parts.push_back(part);
      current_path += part + "/";
      response->print("<a href=\"");
      response->print(this->build_prefix().c_str());
      response->print(current_path.c_str());
      response->print("\">");
      response->print("<svg class=\"folder-icon\" viewBox=\"0 0 24 24\"><path fill=\"currentColor\" d=\"M10 4H4c-1.1 0-1.99.9-1.99 2L2 18c0 1.1.9 2 2 2h16c1.1 0 2-.9 2-2V8c0-1.1-.9-2-2-2h-8l-2-2z\"/></svg>");
      response->print(part.c_str());
      response->print("</a> > ");
    }
    relative_path.erase(0, pos + 1);
  }

  response->print(F(R"(
    </div>
    )"));

  if (this->upload_enabled_) {
    response->print(F(R"(
    <div class="upload-form">
      <form method="POST" enctype="multipart/form-data">
        <input type="file" name="file" required>
        <button type="submit">Upload File</button>
      </form>
    </div>
    )"));
  }

  response->print(F(R"(
    <table>
      <thead>
        <tr>
          <th>Name</th>
          <th>Type</th>
          <th>Size</th>
          <th>Actions</th>
        </tr>
      </thead>
      <tbody>
  )"));

  auto entries = this->sd_mmc_card_->list_directory_file_info(path, 0);
  for (auto const &entry : entries) {
    std::string uri = "/" + Path::join(this->url_prefix_, Path::remove_root_path(entry.path, this->root_path_));
    std::string file_name = Path::file_name(entry.path);
    response->print("<tr><td>");
    if (entry.is_directory) {
      response->print("<a class=\"folder\" href=\"");
      response->print(uri.c_str());
      response->print("\">");
      response->print("<svg class=\"folder-icon\" viewBox=\"0 0 24 24\"><path fill=\"currentColor\" d=\"M10 4H4c-1.1 0-1.99.9-1.99 2L2 18c0 1.1.9 2 2 2h16c1.1 0 2-.9 2-2V8c0-1.1-.9-2-2-2h-8l-2-2z\"/></svg>");
      response->print(file_name.c_str());
      response->print("</a>");
    } else {
      response->print(file_name.c_str());
    }
    response->print("</td><td>");
    if (entry.is_directory) {
      response->print("Folder");
    } else {
      response->print("<span class=\"file-type\">");
      response->print(get_file_type(file_name).c_str());
      response->print("</span>");
    }
    response->print("</td><td>");
    if (!entry.is_directory) {
      response->print(format_size(entry.size).c_str());
    }
    response->print("</td><td class=\"file-actions\">");
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

  response->print(F(R"(
      </tbody>
    </table>
    <script>
      function delete_file(path) {
        if (confirm('Are you sure you want to delete this file?')) {
          fetch(path, {method: 'DELETE'})
            .then(response => {
              if (response.ok) location.reload();
              else alert('Failed to delete file');
            });
        }
      }
      function download_file(path, filename) {
        fetch(path)
          .then(response => response.blob())
          .then(blob => {
            const link = document.createElement('a');
            link.href = URL.createObjectURL(blob);
            link.download = filename;
            link.click();
          })
          .catch(console.error);
      }
    </script>
  </div>
</body>
</html>
  )"));

  request->send(response);
}

void SDFileServer::handle_download(AsyncWebServerRequest *request, std::string const &path) const {
  if (!this->download_enabled_) {
    request->send(401, "application/json", "{ \"error\": \"file download is disabled\" }");
    return;
  }

  size_t file_size = this->sd_mmc_card_->file_size(path);
  if (file_size == 0) {
    request->send(404, "application/json", "{ \"error\": \"file not found or empty\" }");
    return;
  }

  std::string file_name = Path::file_name(path);
  std::string content_type = "application/octet-stream";

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

  auto file = this->sd_mmc_card_->read_file(path);
  if (file.size() == 0) {
    request->send(401, "application/json", "{ \"error\": \"failed to read file\" }");
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
    request->send(401, "application/json", "{ \"error\": \"can not delete directory\" }");
    return;
  }

  if (this->sd_mmc_card_->delete_file(path)) {
    request->send(200, "application/json", "{ \"message\": \"file deleted\" }");
  } else {
    request->send(401, "application/json", "{ \"error\": \"file deletion failed\" }");
  }
}

std::string SDFileServer::build_prefix() const { return "/" + this->url_prefix_; }
std::string SDFileServer::extract_path_from_url(std::string const &url) const {
  std::string prefix = this->build_prefix();
  std::string path = url.substr(prefix.length());
  return path;
}
std::string SDFileServer::build_absolute_path(std::string file_path) const {
  std::string path = Path::join(this->root_path_, file_path);
  if (!Path::is_absolute(path)) {
    path = Path::separator + path;
  }
  return path;
}

}  // namespace sd_file_server
}  // namespace esphome











































































