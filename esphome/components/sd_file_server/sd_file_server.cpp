#include "sd_file_server.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include <esp_http_server.h>
#include <map>
#include <algorithm>
#include <sstream>

namespace esphome {
namespace sd_file_server {

static const char *const TAG = "sd_file_server";

// Helper function to format file sizes
std::string format_size(size_t size) {
  const char* units[] = {"B", "KB", "MB", "GB"};
  int unit = 0;
  double size_d = static_cast<double>(size);
  while (size_d >= 1024 && unit < 3) {
    size_d /= 1024;
    unit++;
  }
  char buffer[32];
  snprintf(buffer, sizeof(buffer), "%.2f %s", size_d, units[unit]);
  return std::string(buffer);
}

// Helper class for path operations
class Path {
public:
  static std::string file_name(const std::string& path) {
    size_t pos = path.find_last_of("/\\");
    return (pos == std::string::npos) ? path : path.substr(pos + 1);
  }
};

SDFileServer::SDFileServer() {}

void SDFileServer::setup() {
  ESP_LOGCONFIG(TAG, "Setting up SD File Server...");
  if (this->sd_mmc_card_ == nullptr) {
    ESP_LOGE(TAG, "SD card not configured!");
    this->mark_failed();
    return;
  }
  this->start_server();
}

void SDFileServer::loop() {
  // Nothing to do here
}

void SDFileServer::dump_config() {
  ESP_LOGCONFIG(TAG, "SD File Server:");
  ESP_LOGCONFIG(TAG, "  Port: %u", this->port_);
  ESP_LOGCONFIG(TAG, "  URL Prefix: %s", this->url_prefix_.c_str());
  ESP_LOGCONFIG(TAG, "  Root Path: %s", this->root_path_.c_str());
  ESP_LOGCONFIG(TAG, "  Deletion Enabled: %s", YESNO(this->deletion_enabled_));
  ESP_LOGCONFIG(TAG, "  Download Enabled: %s", YESNO(this->download_enabled_));
  ESP_LOGCONFIG(TAG, "  Upload Enabled: %s", YESNO(this->upload_enabled_));
}

void SDFileServer::start_server() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = this->port_;
  config.uri_match_fn = httpd_uri_match_wildcard;

  ESP_LOGD(TAG, "Starting HTTP Server on port %d", config.server_port);
  esp_err_t err = httpd_start(&this->server_, &config);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to start HTTP Server!");
    this->mark_failed();
    return;
  }

  httpd_uri_t index = {
      .uri = (this->url_prefix_ + "/*").c_str(),
      .method = HTTP_GET,
      .handler = SDFileServer::index_handler,
      .user_ctx = this
  };
  httpd_register_uri_handler(this->server_, &index);

  if (this->download_enabled_) {
    httpd_uri_t file = {
        .uri = (this->url_prefix_ + "/*").c_str(),
        .method = HTTP_GET,
        .handler = SDFileServer::file_handler,
        .user_ctx = this
    };
    httpd_register_uri_handler(this->server_, &file);
  }

  if (this->deletion_enabled_) {
    httpd_uri_t del = {
        .uri = (this->url_prefix_ + "/*").c_str(),
        .method = HTTP_DELETE,
        .handler = SDFileServer::delete_handler,
        .user_ctx = this
    };
    httpd_register_uri_handler(this->server_, &del);
  }

  if (this->upload_enabled_) {
    httpd_uri_t upload = {
        .uri = (this->url_prefix_ + "/*").c_str(),
        .method = HTTP_POST,
        .handler = SDFileServer::upload_handler,
        .user_ctx = this
    };
    httpd_register_uri_handler(this->server_, &upload);
  }

  ESP_LOGD(TAG, "HTTP Server started successfully");
}

esp_err_t SDFileServer::index_handler(httpd_req_t *req) {
  SDFileServer *server = static_cast<SDFileServer*>(req->user_ctx);
  std::string path = server->extract_path_from_url(req->uri);
  std::string full_path = server->build_absolute_path(path);

  if (!server->sd_mmc_card_->is_directory(full_path)) {
    return server->file_handler(req);
  }

  std::string content = R"(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>SD Card Files</title>
  <style>
    body { font-family: Arial, sans-serif; margin: 0; padding: 20px; background: #f0f0f0; }
    .container { max-width: 800px; margin: 0 auto; background: white; padding: 20px; border-radius: 5px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }
    h1 { color: #333; }
    table { width: 100%; border-collapse: collapse; }
    th, td { padding: 10px; text-align: left; border-bottom: 1px solid #ddd; }
    th { background-color: #f2f2f2; }
    .file-actions { display: flex; gap: 10px; }
    .file-actions button { padding: 5px 10px; cursor: pointer; }
  </style>
</head>
<body>
  <div class="container">
    <h1>SD Card Files</h1>
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
  )";

  auto entries = server->sd_mmc_card_->list_directory_file_info(full_path, 0);
  for (const auto &entry : entries) {
    std::string file_name = Path::file_name(entry.path);
    std::string file_type = entry.is_directory ? "Directory" : "File";
    std::string file_size = entry.is_directory ? "-" : format_size(entry.size);
    
    content += "<tr>";
    content += "<td>" + file_name + "</td>";
    content += "<td>" + file_type + "</td>";
    content += "<td>" + file_size + "</td>";
    content += "<td class='file-actions'>";
    if (!entry.is_directory && server->download_enabled_) {
      content += "<button onclick=\"downloadFile('" + file_name + "')\">Download</button>";
    }
    if (server->deletion_enabled_) {
      content += "<button onclick=\"deleteFile('" + file_name + "')\">Delete</button>";
    }
    content += "</td></tr>";
  }

  content += R"(
      </tbody>
    </table>
    <script>
      function downloadFile(fileName) {
        window.location.href = fileName;
      }
      function deleteFile(fileName) {
        if (confirm('Are you sure you want to delete ' + fileName + '?')) {
          fetch(fileName, { method: 'DELETE' })
            .then(response => {
              if (response.ok) {
                alert('File deleted successfully');
                location.reload();
              } else {
                alert('Failed to delete file');
              }
            });
        }
      }
    </script>
  </div>
</body>
</html>
  )";

  httpd_resp_set_type(req, "text/html");
  httpd_resp_send(req, content.c_str(), content.length());
  return ESP_OK;
}

esp_err_t SDFileServer::file_handler(httpd_req_t *req) {
  SDFileServer *server = static_cast<SDFileServer*>(req->user_ctx);
  std::string path = server->extract_path_from_url(req->uri);
  std::string full_path = server->build_absolute_path(path);

  if (!server->sd_mmc_card_->exists(full_path)) {
    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File not found");
    return ESP_FAIL;
  }

  FILE* file = fopen(full_path.c_str(), "rb");
  if (file == NULL) {
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to open file");
    return ESP_FAIL;
  }

  char *chunk = (char*)malloc(1024);
  size_t chunksize;
  do {
    chunksize = fread(chunk, 1, 1024, file);
    if (httpd_resp_send_chunk(req, chunk, chunksize) != ESP_OK) {
      free(chunk);
      fclose(file);
      httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
      return ESP_FAIL;
    }
  } while (chunksize != 0);

  free(chunk);
  fclose(file);
  httpd_resp_send_chunk(req, NULL, 0);
  return ESP_OK;
}

esp_err_t SDFileServer::delete_handler(httpd_req_t *req) {
  SDFileServer *server = static_cast<SDFileServer*>(req->user_ctx);
  if (!server->deletion_enabled_) {
    httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "File deletion is disabled");
    return ESP_FAIL;
  }

  std::string path = server->extract_path_from_url(req->uri);
  std::string full_path = server->build_absolute_path(path);

  if (server->sd_mmc_card_->is_directory(full_path)) {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Cannot delete directories");
    return ESP_FAIL;
  }

  if (server->sd_mmc_card_->delete_file(full_path)) {
    httpd_resp_sendstr(req, "File deleted successfully");
    return ESP_OK;
  } else {
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to delete file");
    return ESP_FAIL;
  }
}

esp_err_t SDFileServer::upload_handler(httpd_req_t *req) {
  SDFileServer *server = static_cast<SDFileServer*>(req->user_ctx);
  if (!server->upload_enabled_) {
    httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "File upload is disabled");
    return ESP_FAIL;
  }

  char content[1024];
  int received;

  std::string path = server->extract_path_from_url(req->uri);
  std::string full_path = server->build_absolute_path(path);

  FILE* file = fopen(full_path.c_str(), "wb");
  if (file == NULL) {
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to create file");
    return ESP_FAIL;
  }

  while ((received = httpd_req_recv(req, content, sizeof(content))) > 0) {
    if (fwrite(content, 1, received, file) != received) {
      fclose(file);
      httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to write file");
      return ESP_FAIL;
    }
  }

  fclose(file);

  if (received == HTTPD_SOCK_ERR_TIMEOUT) {
    httpd_resp_send_408(req);
  } else {
    httpd_resp_sendstr(req, "File uploaded successfully");
  }

  return ESP_OK;
}

std::string SDFileServer::build_prefix() const {
  return "/" + this->url_prefix_;
}

std::string SDFileServer::extract_path_from_url(const std::string &url) const {
  std::string prefix = this->build_prefix();
  return url.substr(prefix.length());
}

std::string SDFileServer::build_absolute_path(const std::string &file_path) const {
  std::string path = file_path;
  if (!path.empty() && path[0] != '/') {
    path = "/" + path;
  }
  return this->root_path_ + path;
}

}  // namespace sd_file_server
}  // namespace esphome










































































