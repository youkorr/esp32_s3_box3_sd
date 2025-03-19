#include "sd_file_server.h"
#include "esphome/core/application.h"
#include "esphome/core/http.h"
#include <FS.h>
#include <HTTPClient.h>
#include "esphome/core/path.h"

#include <Update.h>

namespace esphome {
namespace sd_file_server {

static const char *const TAG = "sd_file_server";

class ChunkedFileResponse : public web_server_idf::AsyncWebServerResponse {
 private:
  std::string path_;
  size_t chunk_size_;
  std::string mime_type_;
  File file_;
  size_t file_size_;
  size_t current_position_;

 public:
  ChunkedFileResponse(const std::string& path, const std::string& mime_type, size_t chunk_size, web_server_idf::AsyncWebServerRequest *request);
  ~ChunkedFileResponse() override;

 protected:
  bool _sourceValid() const override;
  void _respond(web_server_idf::AsyncWebServerRequest* request) override;
  size_t _fillBuffer(uint8_t* data, size_t len) override;
  const char *get_content_data() const override;
  size_t get_content_size() const override;
};

ChunkedFileResponse::ChunkedFileResponse(const std::string& path, const std::string& mime_type, size_t chunk_size, web_server_idf::AsyncWebServerRequest *request)
    : AsyncWebServerResponse(request),
      path_(path),
      chunk_size_(chunk_size),
      mime_type_(mime_type),
      current_position_(0) {
  if (!SD.exists(path_.c_str())) {
    ESP_LOGW(TAG, "File not found: %s", path_.c_str());
    file_ = nullptr;
    _code = 404; // Set the status code here
  } else {
    file_ = SD.open(path_.c_str(), "r");
    if (!file_) {
      ESP_LOGE(TAG, "Failed to open file: %s", path_.c_str());
      _code = 500; // Internal Server Error
    } else {
      file_size_ = file_.size();
      _code = 200; // OK
      setContentType(mime_type_.c_str());
      setContentLength(file_size_);
      _sendContentLength = true;
      _chunked = true;

      ESP_LOGD(TAG, "Creating ChunkedFileResponse for %s, size=%zu", path_.c_str(), file_size_);
    }
  }
}

ChunkedFileResponse::~ChunkedFileResponse() {
  if (file_) {
    file_.close();
  }
}

bool ChunkedFileResponse::_sourceValid() const {
  return file_ && current_position_ < file_size_;
}

void ChunkedFileResponse::_respond(web_server_idf::AsyncWebServerRequest* request) {
  // No need to call the base class _respond. Just implement the chunking logic here.
  // This method will be called repeatedly until _sourceValid() returns false.
}

size_t ChunkedFileResponse::_fillBuffer(uint8_t* data, size_t len) {
  if (!file_) return 0;

  size_t bytes_to_read = std::min(len, chunk_size_);
  bytes_to_read = std::min(bytes_to_read, file_size_ - current_position_);

  file_.seek(current_position_);
  size_t bytes_read = file_.read(data, bytes_to_read);
  current_position_ += bytes_read;

  return bytes_read;
}

const char *ChunkedFileResponse::get_content_data() const {
    // This should return a pointer to the beginning of the data to send.
    // In this chunked implementation, we don't need to return the whole data at once,
    // so we can return nullptr.  The _fillBuffer method handles providing the data.
    return nullptr;
}

size_t ChunkedFileResponse::get_content_size() const {
    // This should return the total size of the content.
    // Since we are using chunked transfer, we can return 0 here.
    return file_size_;
}

SDFileServer::SDFileServer(SdMmc *sd_mmc_card) : sd_mmc_card_(sd_mmc_card) {
    web_server_ = App.get_web_server();
}

void SDFileServer::setup() {
  this->web_server_->on(this->base_path_.c_str(), HTTP_GET, [this](web_server_idf::AsyncWebServerRequest *request) {
    this->handleRequest(request);
  });

  this->web_server_->on(
      this->base_path_.c_str(), HTTP_POST, [](web_server_idf::AsyncWebServerRequest *request) {}, nullptr,
      [this](web_server_idf::AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index,
             size_t total) {
        this->handleUpload(request, data, len, index, total);
      });

  ESP_LOGI(TAG, "SD File Server setup on %s", this->base_path_.c_str());
}

void SDFileServer::handleRequest(web_server_idf::AsyncWebServerRequest *request) {
  const String path = request->url();
  if (SD.exists(path.c_str())) {
    if (SD.isDir(path.c_str())) {
      this->handle_directory_listing(request, path.c_str());
    } else {
      this->handle_download(request, path.c_str());
    }
  } else {
    request->send(404, "text/plain", "Not found");
  }
}

void SDFileServer::handle_directory_listing(web_server_idf::AsyncWebServerRequest *request, const std::string& path) {
  String output = "<html><head><title>ESPHome SD Card</title></head><body><h1>" + String(path.c_str()) +
                  "</h1><hr><ul>";
  File dir = SD.open(path.c_str());
  if (!dir) {
    request->send(500, "text/plain", "Failed to open directory");
    return;
  }
  File file = dir.openNextFile();
  while (file) {
    String filename = file.name();
    if (filename.startsWith(".")) {
      file = dir.openNextFile();
      continue;
    }
    output += "<li><a href=\"" + filename + "\">" + filename + "</a>";
    if (this->delete_enabled_) {
      output += " <a href=\"?delete=" + filename + "\">Delete</a>";
    }
    output += "</li>";
    file = dir.openNextFile();
  }
  output += "</ul><hr></body></html>";
  request->send(200, "text/html", output);
}

void SDFileServer::handle_download(web_server_idf::AsyncWebServerRequest *request, const std::string& path) const {
  ESP_LOGD(TAG, "handle_download path=%s", path.c_str());
  auto *response = new ChunkedFileResponse(path, Path::mime_type(path), chunk_size, request);

    request->send(response);

}

void SDFileServer::handleUpload(web_server_idf::AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index,
                                size_t total) {
  String path = request->url();
  path.replace("%20", " ");
  if (!index) {
    ESP_LOGI(TAG, "Upload Start: %s\n", path.c_str());
    // open the file in write mode
    this->upload_file_ = SD.open(path.c_str(), FILE_WRITE);
    if (!this->upload_file_) {
      ESP_LOGE(TAG, "Failed to open file for writing");
      request->send(500, "text/plain", "Failed to open file for writing");
      return;
    }
  }

  if (len) {
    // Write data to the file
    this->upload_file_.write(data, len);
  }

  if (index + len == total) {
    // close the file when upload is complete
    this->upload_file_.close();
    ESP_LOGI(TAG, "Upload complete: %s, size: %d\n", path.c_str(), total);
    request->send(200, "text/plain", "OK");
  }
}

void SDFileServer::handle_delete(web_server_idf::AsyncWebServerRequest *request) {
  String path = request->arg("delete");
  if (path.length() == 0) {
    request->send(400, "text/plain", "Missing 'delete' argument");
    return;
  }
  String full_path = this->base_path_ + "/" + path;
  if (!SD.exists(full_path.c_str())) {
    request->send(404, "text/plain", "File not found");
    return;
  }
  if (SD.remove(full_path.c_str())) {
    request->send(200, "text/plain", "OK");
  } else {
    request->send(500, "text/plain", "Failed to delete file");
  }
}

void SDFileServer::set_base_path(std::string base_path) { this->base_path_ = base_path; }
void SDFileServer::set_delete_enabled(bool delete_enabled) { this->delete_enabled_ = delete_enabled; }

}  // namespace sd_file_server
}  // namespace esphome














































































