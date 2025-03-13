#include "sd_file_server.h"
#include "esphome/core/log.h"
#include "esphome/components/network/util.h"
#include "esphome/core/helpers.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace sd_file_server {

static const char *TAG = "sd_file_server";

// Définir la taille du buffer pour le transfer chunked
static const size_t CHUNK_SIZE = 4096;  // Taille optimale pour ESP32-S3

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
      {"mp3", "Audio (MP3)"},   {"wav", "Audio (WAV)"},   {"png", "Image (PNG)"},   {"jpg", "Image (JPG)"},
      {"jpeg", "Image (JPEG)"}, {"bmp", "Image (BMP)"},   {"txt", "Text (TXT)"},   {"log", "Text (LOG)"},
      {"csv", "Text (CSV)"},   {"html", "Web (HTML)"},   {"css", "Web (CSS)"},   {"js", "Web (JS)"},
      {"json", "Data (JSON)"},  {"xml", "Data (XML)"},   {"zip", "Archive (ZIP)"}, {"gz", "Archive (GZ)"},
      {"tar", "Archive (TAR)"}};
  size_t dot_pos = filename.rfind('.');
  if (dot_pos != std::string::npos) {
    std::string ext = filename.substr(dot_pos + 1);
    auto it = file_types.find(ext);
    if (it != file_types.end()) {
      return it->second;
    }
  }
  return "File (" + filename.substr(dot_pos + 1) + ")";
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

bool Path::trailing_slash(std::string const &path) { return path.size() && path[path.length() - 1] == separator; }

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
void SDFileServer::setup() { this->base_->add_handler(this); }
void SDFileServer::dump_config() {
  ESP_LOGCONFIG(TAG, "SD File Server:");
  ESP_LOGCONFIG(TAG, "  Address: %s:%u", network::get_use_address().c_str(), this->base_->get_port());
  ESP_LOGCONFIG(TAG, "  Url Prefix: %s", this->url_prefix_.c_str());
  ESP_LOGCONFIG(TAG, "  Root Path: %s", this->root_path_.c_str());
  ESP_LOGCONFIG(TAG, "  Deletation Enabled: %s", TRUEFALSE(this->deletion_enabled_));
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

void SDFileServer::handle_index(AsyncWebServerRequest *request, std::string const &path) const {
  AsyncResponseStream *response = request->beginResponseStream("text/html");

  response->print(F(R"(
    SD Card Files
    SD Card Files
    Accéder à Web Server
    Home > files > )"));

  // Breadcrumb navigation
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
      response->print("");
      response->print(part.c_str());
      response->print(" > ");
      response->print("");
    }
    relative_path.erase(0, pos + 1);
  }

  response->print(F(R"(

    Upload File

    Name
    Type
    Size
    Actions

  )"));

  auto entries = this->sd_mmc_card_->list_directory_file_info(path, 0);
  for (auto const &entry : entries) {
    std::string uri = "/" + Path::join(this->url_prefix_, Path::remove_root_path(entry.path, this->root_path_));
    std::string file_name = Path::file_name(entry.path);
    response->print("");
    if (entry.is_directory) {
      response->print("<a href=\"");
      response->print(uri.c_str());
      response->print("\">");
      response->print("");
      response->print(file_name.c_str());
      response->print("");
    } else {
      response->print(file_name.c_str());
    }
    response->print("");
    if (entry.is_directory) {
      response->print("Folder");
    } else {
      response->print("");
      response->print(get_file_type(file_name).c_str());
      response->print("");
    }
    response->print("");
    if (!entry.is_directory) {
      response->print(format_size(entry.size).c_str());
    }
    response->print("");
    if (!entry.is_directory) {
      if (this->download_enabled_) {
        response->print("<a href=\"#\" onclick=\"downloadFile('");
        response->print(uri.c_str());
        response->print("','");
        response->print(file_name.c_str());
        response->print("')\">Download");
      }
      if (this->deletion_enabled_) {
        response->print("<a href=\"#\" onclick=\"deleteFile('");
        response->print(uri.c_str());
        response->print("')\">Delete");
      }
    }
    response->print("");
  }

  response->print(F(R"(

  )"));

  request->send(response);
}

// Nouvelle classe qui sert de callback pour le téléchargement de fichiers en mode chunked
class ChunkedFileResponse : public AsyncWebServerResponse {
 private:
  sd_mmc_card::SdMmc *sd_mmc_card_;
  std::string path_;
  size_t file_size_;
  size_t sent_;
  // Assuming sd_mmc_card::SdMmc provides a method to open a file and returns a compatible file object
  void* file_; // Changed to void*

 public:
  ChunkedFileResponse(const std::string &content_type, sd_mmc_card::SdMmc *sd_mmc_card, const std::string &path,
                      size_t content_length)
      : AsyncWebServerResponse(), sd_mmc_card_(sd_mmc_card), path_(path), file_size_(content_length), sent_(0), file_(nullptr) {
    _contentType = content_type.c_str();
    _contentLength = content_length;
    _sendContentLength = true;
    _chunked = true;
  }

  ~ChunkedFileResponse() {
    if (file_) {
      sd_mmc_card_->close_file(file_); // Assuming sd_mmc_card has close_file method
    }
  }

  const char *get_content_data() const override { return nullptr; }
  size_t get_content_size() const override { return _contentLength; }

  bool _sourceValid() const { return true; }
  size_t _fillBuffer(uint8_t *buffer, size_t maxLen) override {
    if (!file_) {
      file_ = sd_mmc_card_->open_file(path_.c_str(), "r");
      if (!file_) {
        return 0;
      }
    }

    size_t bytesToRead = (maxLen < CHUNK_SIZE) ? maxLen : CHUNK_SIZE;
    bytesToRead = (bytesToRead < (file_size_ - sent_)) ? bytesToRead : (file_size_ - sent_);
    if (bytesToRead == 0) {
      return 0;
    }

    size_t read = sd_mmc_card_->read_file_data(file_, buffer, bytesToRead); // Assuming sd_mmc_card has read_file_data method
    if (read > 0) {
      sent_ += read;
    }

    // Fermer le fichier si nous avons fini
    if (sent_ >= file_size_) {
      sd_mmc_card_->close_file(file_); // Assuming sd_mmc_card has close_file method
      file_ = nullptr;
    }

    return read;
  }
};

void SDFileServer::handle_download(AsyncWebServerRequest *request, std::string const &path) const {
  if (!this->download_enabled_) {
    request->send(401, "application/json", "{ \"error\": \"file download is disabled\" }");
    return;
  }

  // Vérifier si le fichier existe
  size_t file_size = this->sd_mmc_card_->file_size(path);
  if (file_size == 0) {
    request->send(404, "application/json", "{ \"error\": \"file not found or empty\" }");
    return;
  }

  // Déterminer le type de contenu basé sur l'extension du fichier
  std::string file_name = Path::file_name(path);
  std::string content_type = "application/octet-stream";  // Type de contenu par défaut

  // Obtenir l'extension du fichier et définir le type de contenu approprié
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

  // Si le fichier est plus petit que 64KB, utilisez la méthode d'origine
  if (file_size < 65536) {
    // Utiliser l'approche de lecture de fichier existante pour les petits fichiers
    auto file = this->sd_mmc_card_->read_file(path);
    if (file.size() == 0) {
      request->send(401, "application/json", "{ \"error\": \"failed to read file\" }");
      return;
    }

#ifdef USE_ESP_IDF
    auto *response = request->beginResponse_P(200, content_type.c_str(), file.data(), file.size());
#else
    auto *response = request->beginResponseStream(content_type.c_str(), file.size());
    response->write(file.data(), file.size());
#endif
    request->send(response);
    return;
  }

  // Pour les fichiers volumineux, utiliser le transfert chunked
  ESP_LOGI(TAG, "Serving large file (%s) in chunked mode, size: %zu bytes", path.c_str(), file_size);
  ChunkedFileResponse *response = new ChunkedFileResponse(content_type, this->sd_mmc_card_, path, file_size);
  response->addHeader("Content-Disposition", ("attachment; filename=\"" + file_name + "\"").c_str());
  response->addHeader("Accept-Ranges", "bytes");
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
  // Normalize root path
  std::string normalized_root = root_path_;
  if (!Path::trailing_slash(normalized_root)) {
    normalized_root += Path::separator;
  }

  // Handle empty relative path
  if (relative_path.empty()) {
    return normalized_root;
  }

  // Remove leading slash from relative path if present
  if (Path::is_absolute(relative_path)) {
    relative_path = relative_path.substr(1);
  }

  return normalized_root + relative_path;
}

}  // namespace sd_file_server
}  // namespace esphome





































