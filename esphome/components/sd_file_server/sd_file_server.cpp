#include "sd_file_server.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/util.h"
#include "path.h"

namespace esphome {
namespace sd_file_server {

static const char *const TAG = "sd_file_server";

class ChunkedFileResponse : public AsyncWebServerResponse {
private:
  std::string path_;
  FILE* file_;
  size_t file_size_;
  size_t position_;
  size_t chunk_size_;
  uint8_t* buffer_;
  std::string mime_type_;

public:
  ChunkedFileResponse(const std::string& path, const std::string& mime_type, size_t chunk_size = 8192)
      : path_(path), file_(nullptr), file_size_(0), position_(0), chunk_size_(chunk_size), 
        mime_type_(mime_type) {
    buffer_ = new uint8_t[chunk_size_];
    
    // Ouvrir le fichier et obtenir sa taille
    file_ = fopen(path_.c_str(), "rb");
    if (file_ != nullptr) {
      fseek(file_, 0, SEEK_END);
      file_size_ = ftell(file_);
      fseek(file_, 0, SEEK_SET);
      
      ESP_LOGI(TAG, "Preparing chunked download: %s (%zu bytes)", path_.c_str(), file_size_);
      
      _contentType = mime_type_;
      _contentLength = file_size_;
      _code = 200;
      _sendContentLength = true;
      _chunked = true;
    } else {
      ESP_LOGE(TAG, "Failed to open file for reading: %s", path_.c_str());
      _code = 404;
    }
  }
  
  ~ChunkedFileResponse() {
    if (file_ != nullptr) {
      fclose(file_);
    }
    delete[] buffer_;
  }
  
  bool _sourceValid() const override {
    return (file_ != nullptr);
  }
  
  void _respond(AsyncWebServerRequest* request) override {
    ESP_LOGI(TAG, "Starting chunked file download: %s", path_.c_str());
    AsyncWebServerResponse::_respond(request);
  }
  
  size_t _fillBuffer(uint8_t* data, size_t len) override {
    if (file_ == nullptr || position_ >= file_size_) {
      return 0;
    }
    
    size_t to_read = std::min(len, file_size_ - position_);
    size_t bytes_read = fread(data, 1, to_read, file_);
    position_ += bytes_read;
    
    if (bytes_read == 0 && position_ < file_size_) {
      ESP_LOGE(TAG, "Error reading file at position %zu", position_);
    }
    
    return bytes_read;
  }
};

SDFileServer::SDFileServer(SdMmc *sd_mmc_card) : sd_mmc_card_(sd_mmc_card) {
  this->web_server_ = global_web_server;
  this->base_path_ = "/sd";
  this->download_enabled_ = true;
  this->upload_enabled_ = true;
  this->delete_enabled_ = true;
}

void SDFileServer::setup() {
  this->web_server_->on(this->base_path_.c_str(), HTTP_GET, [this](AsyncWebServerRequest *request) {
    this->handle_request(request);
  });

  this->web_server_->on(
      this->base_path_.c_str(), HTTP_POST, [](AsyncWebServerRequest *request) {}, nullptr,
      [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        this->handle_upload(request, data, len, index, total);
      });

  ESP_LOGI(TAG, "SD File Server setup on %s", this->base_path_.c_str());
}

void SDFileServer::handle_request(AsyncWebServerRequest *request) {
  if (!this->sd_mmc_card_->is_mounted()) {
    request->send(500, "application/json", "{ \"error\": \"SD card not mounted\" }");
    return;
  }

  std::string path = request->url().c_str();
  if (path.find(this->base_path_) == 0) {
    path = path.substr(this->base_path_.length());
  }

  if (path.empty() || path == "/") {
    this->handle_directory_listing(request, "/");
    return;
  }

  if (request->hasParam("download")) {
    this->handle_download(request, path);
    return;
  }

  if (request->hasParam("delete")) {
    this->handle_delete(request, path);
    return;
  }

  if (this->sd_mmc_card_->is_directory(path)) {
    this->handle_directory_listing(request, path);
    return;
  }

  this->handle_download(request, path);
}

void SDFileServer::handle_directory_listing(AsyncWebServerRequest *request, std::string const &path) const {
  std::vector<std::string> directories;
  std::vector<std::string> files;

  if (!this->sd_mmc_card_->list_directory(path, directories, files)) {
    request->send(500, "application/json", "{ \"error\": \"failed to list directory\" }");
    return;
  }

  std::string response = "<!DOCTYPE html><html><head><title>SD Card Directory Listing</title>";
  response += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
  response += "<style>";
  response += "body { font-family: Arial, sans-serif; margin: 20px; }";
  response += "h1 { color: #333; }";
  response += "ul { list-style-type: none; padding: 0; }";
  response += "li { margin: 10px 0; padding: 10px; background-color: #f5f5f5; border-radius: 5px; }";
  response += "a { text-decoration: none; color: #0066cc; }";
  response += ".directory { font-weight: bold; }";
  response += ".actions { margin-left: 20px; display: inline-block; }";
  response += ".actions a { margin-right: 10px; padding: 5px; border: 1px solid #ddd; border-radius: 3px; }";
  response += ".upload-form { margin: 20px 0; padding: 15px; background-color: #f0f0f0; border-radius: 5px; }";
  response += "</style>";
  response += "</head><body>";
  response += "<h1>Directory: " + path + "</h1>";
  response += "<ul>";

  if (path != "/") {
    std::string parent = Path::parent_path(path);
    response += "<li><a href=\"" + this->base_path_ + parent + "\" class=\"directory\">..</a></li>";
  }

  for (const auto &directory : directories) {
    std::string full_path = path + (path.back() == '/' ? "" : "/") + directory;
    response += "<li><a href=\"" + this->base_path_ + full_path + "\" class=\"directory\">" + directory + "</a></li>";
  }

  for (const auto &file : files) {
    std::string full_path = path + (path.back() == '/' ? "" : "/") + file;
    response += "<li><a href=\"" + this->base_path_ + full_path + "\">" + file + "</a>";
    response += "<div class=\"actions\">";
    response += "<a href=\"" + this->base_path_ + full_path + "?download\">Download</a>";
    if (this->delete_enabled_) {
      response += "<a href=\"" + this->base_path_ + full_path + "?delete\" onclick=\"return confirm('Are you sure?');\">Delete</a>";
    }
    response += "</div></li>";
  }

  response += "</ul>";

  if (this->upload_enabled_) {
    response += "<div class=\"upload-form\">";
    response += "<h2>Upload File</h2>";
    response += "<form method=\"POST\" action=\"" + this->base_path_ + path + "\" enctype=\"multipart/form-data\">";
    response += "<input type=\"file\" name=\"file\"><br><br>";
    response += "<input type=\"submit\" value=\"Upload\">";
    response += "</form></div>";
  }

  response += "</body></html>";

  request->send(200, "text/html", response.c_str());
}

void SDFileServer::handle_download(AsyncWebServerRequest *request, std::string const &path) const {
  if (!this->download_enabled_) {
    request->send(401, "application/json", "{ \"error\": \"file download is disabled\" }");
    return;
  }
  
  // Vérifier que le fichier existe
  if (!this->sd_mmc_card_->exists(path)) {
    request->send(404, "application/json", "{ \"error\": \"file not found\" }");
    return;
  }
  
  // Pour les petits fichiers (< 16KB), utilisez la méthode existante pour des performances optimales
  size_t file_size = this->sd_mmc_card_->get_file_size(path);
  if (file_size < 16 * 1024) {
    auto file = this->sd_mmc_card_->read_file(path);
    if (file.size() == 0) {
      request->send(500, "application/json", "{ \"error\": \"failed to read file\" }");
      return;
    }
#ifdef USE_ESP_IDF
    auto *response = request->beginResponse_P(200, Path::mime_type(path).c_str(), file.data(), file.size());
#else
    auto *response = request->beginResponseStream(Path::mime_type(path).c_str(), file.size());
    response->write(file.data(), file.size());
#endif
    request->send(response);
    return;
  }
  
  // Pour les fichiers volumineux, utilisez le téléchargement par chunks
  ESP_LOGI(TAG, "Using chunked download for large file: %s (%zu bytes)", path.c_str(), file_size);
  
  // Définir une taille de chunk raisonnable (8KB est un bon compromis)
  const size_t chunk_size = 8 * 1024;
  
  // Créer et envoyer la réponse par chunks
  auto *response = new ChunkedFileResponse(path, Path::mime_type(path), chunk_size);
  if (!response->_sourceValid()) {
    delete response;
    request->send(500, "application/json", "{ \"error\": \"failed to prepare file for download\" }");
    return;
  }
  
  // Ajouter des en-têtes utiles
  response->addHeader("Content-Disposition", "attachment; filename=\"" + Path::file_name(path) + "\"");
  
  request->send(response);
}

void SDFileServer::handle_upload(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index,
                                 size_t total) {
  if (!this->upload_enabled_) {
    request->send(401, "application/json", "{ \"error\": \"file upload is disabled\" }");
    return;
  }

  if (!this->sd_mmc_card_->is_mounted()) {
    request->send(500, "application/json", "{ \"error\": \"SD card not mounted\" }");
    return;
  }

  static FILE *file = nullptr;
  static std::string upload_path;

  if (index == 0) {
    // Start of upload, open the file
    if (file) {
      fclose(file);
      file = nullptr;
    }

    std::string path = request->url().c_str();
    if (path.find(this->base_path_) == 0) {
      path = path.substr(this->base_path_.length());
    }

    if (path.empty() || path.back() != '/') {
      path += '/';
    }

    upload_path = path + request->arg("file").c_str();
    ESP_LOGI(TAG, "Starting file upload to: %s", upload_path.c_str());

    file = fopen(upload_path.c_str(), "wb");
    if (!file) {
      ESP_LOGE(TAG, "Failed to open file for writing: %s", upload_path.c_str());
      request->send(500, "application/json", "{ \"error\": \"failed to create file\" }");
      return;
    }
  }

  if (file) {
    // Write data to file
    if (len > 0) {
      size_t written = fwrite(data, 1, len, file);
      if (written != len) {
        ESP_LOGE(TAG, "Failed to write to file: %s", upload_path.c_str());
        fclose(file);
        file = nullptr;
        this->sd_mmc_card_->remove_file(upload_path);
        request->send(500, "application/json", "{ \"error\": \"failed to write to file\" }");
        return;
      }
    }

    // End of upload, close the file
    if (index + len == total) {
      fclose(file);
      file = nullptr;
      ESP_LOGI(TAG, "File upload complete: %s (%zu bytes)", upload_path.c_str(), total);

      // Redirect back to the directory
      std::string redirect = this->base_path_ + Path::parent_path(upload_path);
      request->redirect(redirect.c_str());
    }
  }
}

void SDFileServer::handle_delete(AsyncWebServerRequest *request, std::string const &path) const {
  if (!this->delete_enabled_) {
    request->send(401, "application/json", "{ \"error\": \"file deletion is disabled\" }");
    return;
  }

  if (!this->sd_mmc_card_->is_mounted()) {
    request->send(500, "application/json", "{ \"error\": \"SD card not mounted\" }");
    return;
  }

  ESP_LOGI(TAG, "Deleting file: %s", path.c_str());

  if (this->sd_mmc_card_->remove_file(path)) {
    // Redirect back to the parent directory
    std::string redirect = this->base_path_ + Path::parent_path(path);
    request->redirect(redirect.c_str());
  } else {
    request->send(500, "application/json", "{ \"error\": \"failed to delete file\" }");
  }
}

void SDFileServer::set_base_path(std::string base_path) { this->base_path_ = base_path; }
void SDFileServer::set_download_enabled(bool download_enabled) { this->download_enabled_ = download_enabled; }
void SDFileServer::set_upload_enabled(bool upload_enabled) { this->upload_enabled_ = upload_enabled; }
void SDFileServer::set_delete_enabled(bool delete_enabled) { this->delete_enabled_ = delete_enabled; }

}  // namespace sd_file_server
}  // namespace esphome














































































