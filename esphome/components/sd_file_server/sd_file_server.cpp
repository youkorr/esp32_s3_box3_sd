#include "sd_file_server.h"
#include "esphome/core/log.h"
#include "esphome/components/network/util.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace sd_file_server {

void SDFileServer::handle_index(AsyncWebServerRequest *request, std::string const &path) const {
  AsyncResponseStream *response = request->beginResponseStream("text/html");
  response->print(F(R"(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1,user-scalable=no">
  <style>
    body {
      font-family: 'Arial', sans-serif;
      background: linear-gradient(135deg, #1e3c72, #2a5298);
      color: #fff;
      margin: 0;
      padding: 20px;
    }
    h1 {
      text-align: center;
      font-size: 2.5rem;
      margin-bottom: 20px;
    }
    h2 {
      font-size: 1.5rem;
      margin-bottom: 15px;
    }
    table {
      width: 100%;
      border-collapse: collapse;
      background: rgba(255, 255, 255, 0.1);
      border-radius: 10px;
      overflow: hidden;
    }
    th, td {
      padding: 12px;
      text-align: left;
    }
    th {
      background: rgba(255, 255, 255, 0.2);
    }
    tr:hover {
      background: rgba(255, 255, 255, 0.05);
    }
    .file-icon {
      width: 24px;
      height: 24px;
      margin-right: 8px;
      vertical-align: middle;
    }
    .audio-player {
      width: 100%;
      margin-top: 10px;
    }
    .image-preview {
      max-width: 100px;
      max-height: 100px;
      border-radius: 5px;
      transition: transform 0.2s;
    }
    .image-preview:hover {
      transform: scale(1.1);
    }
    button {
      background: #4CAF50;
      border: none;
      color: white;
      padding: 8px 16px;
      border-radius: 5px;
      cursor: pointer;
      transition: background 0.3s;
    }
    button:hover {
      background: #45a049;
    }
    .upload-form {
      margin-bottom: 20px;
    }
    .upload-form input[type="file"] {
      display: none;
    }
    .upload-form label {
      background: #2196F3;
      padding: 10px 20px;
      border-radius: 5px;
      cursor: pointer;
      display: inline-block;
    }
    .upload-form label:hover {
      background: #1e88e5;
    }
  </style>
</head>
<body>
  <h1>📁 SD Card Explorer</h1>
  <h2>📂 Folder: )"));
  response->print(path.c_str());
  response->print(F(R"(
  </h2>
  <form class="upload-form" method="POST" enctype="multipart/form-data">
    <label for="file-upload">⬆️ Upload File</label>
    <input id="file-upload" type="file" name="file">
    <input type="submit" value="Upload">
  </form>
  <a href="/)"));
  response->print(this->url_prefix_.c_str());
  response->print(F(R"(">🏠 Home</a>
  <br><br>
  <table id="files">
    <thead>
      <tr>
        <th>Name</th>
        <th>Actions</th>
      </tr>
    </thead>
    <tbody>)"));
  
  auto entries = this->sd_mmc_card_->list_directory_file_info(path, 0);
  for (auto const &entry : entries) {
    std::string uri = "/" + Path::join(this->url_prefix_, Path::remove_root_path(entry.path, this->root_path_));
    std::string file_name = Path::file_name(entry.path);
    std::string extension = file_name.substr(file_name.find_last_of(".") + 1);

    response->print("<tr><td>");
    if (entry.is_directory) {
      response->print("📁 ");
    } else if (extension == "mp3" || extension == "wav") {
      response->print("🎵 ");
    } else if (extension == "jpg" || extension == "png" || extension == "gif") {
      response->print("🖼️ ");
    } else {
      response->print("📄 ");
    }
    response->print(file_name.c_str());
    response->print("</td><td>");

    if (!entry.is_directory) {
      if (this->download_enabled_) {
        response->print("<button onClick=\"download_file('");
        response->print(uri.c_str());
        response->print("','");
        response->print(file_name.c_str());
        response->print("')\">⬇️ Download</button> ");
      }
      if (this->deletion_enabled_) {
        response->print("<button onClick=\"delete_file('");
        response->print(uri.c_str());
        response->print("')\">🗑️ Delete</button> ");
      }
      if ((extension == "mp3" || extension == "wav") && this->download_enabled_) {
        response->print(R"(<audio class="audio-player" controls><source src=")");
        response->print(uri.c_str());
        response->print(R"(" type="audio/)");
        response->print(extension == "mp3" ? "mpeg" : "wav");
        response->print(R"("></audio>)");
      }
      if ((extension == "jpg" || extension == "png" || extension == "gif") && this->download_enabled_) {
        response->print(R"(<img class="image-preview" src=")");
        response->print(uri.c_str());
        response->print(R"(" alt=")");
        response->print(file_name.c_str());
        response->print(R"(">)");
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
        fetch(path, {method: "DELETE"})
          .then(() => location.reload())
          .catch(console.error);
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
    document.getElementById('file-upload').addEventListener('change', function() {
      if (this.files.length > 0) {
        this.form.submit();
      }
    });
  </script>
</body>
</html>)"));
  request->send(response);
}

void SDFileServer::setup() {
  ESP_LOGCONFIG(TAG, "Setting up SD File Server...");
  this->base_->init();
  this->base_->add_handler(this);
}

void SDFileServer::dump_config() {
  ESP_LOGCONFIG(TAG, "SD File Server:");
  ESP_LOGCONFIG(TAG, "  URL Prefix: %s", this->url_prefix_.c_str());
  ESP_LOGCONFIG(TAG, "  Root Path: %s", this->root_path_.c_str());
  ESP_LOGCONFIG(TAG, "  Deletion Enabled: %s", YESNO(this->deletion_enabled_));
  ESP_LOGCONFIG(TAG, "  Download Enabled: %s", YESNO(this->download_enabled_));
  ESP_LOGCONFIG(TAG, "  Upload Enabled: %s", YESNO(this->upload_enabled_));
}

bool SDFileServer::canHandle(AsyncWebServerRequest *request) {
  return request->url().startsWith(this->url_prefix_.c_str());
}

void SDFileServer::handleRequest(AsyncWebServerRequest *request) {
  std::string path = this->extract_path_from_url(request->url().c_str());
  
  if (request->method() == HTTP_GET) {
    this->handle_get(request);
  } else if (request->method() == HTTP_DELETE) {
    this->handle_delete(request);
  } else {
    request->send(405);
  }
}

void SDFileServer::handleUpload(AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len, bool final) {
  if (!this->upload_enabled_) {
    request->send(403);
    return;
  }

  std::string path = this->extract_path_from_url(request->url().c_str());
  std::string full_path = this->build_absolute_path(path);

  if (index == 0) {
    ESP_LOGD(TAG, "Upload Start: %s", full_path.c_str());
    this->sd_mmc_card_->remove(full_path);
  }

  this->sd_mmc_card_->append(full_path, data, len);

  if (final) {
    ESP_LOGD(TAG, "Upload End: %s (%d bytes)", full_path.c_str(), index + len);
    request->send(200);
  }
}

std::string SDFileServer::build_prefix() const {
  return "/" + this->url_prefix_;
}

std::string SDFileServer::extract_path_from_url(std::string const &url) const {
  std::string path = url.substr(this->build_prefix().length());
  if (path.empty()) {
    path = "/";
  }
  return path;
}

std::string SDFileServer::build_absolute_path(std::string path) const {
  return Path::join(this->root_path_, path);
}

void SDFileServer::handle_get(AsyncWebServerRequest *request) const {
  std::string path = this->extract_path_from_url(request->url().c_str());
  std::string absolute_path = this->build_absolute_path(path);

  if (this->sd_mmc_card_->is_directory(absolute_path)) {
    this->handle_index(request, path);
  } else {
    this->handle_download(request, path);
  }
}

void SDFileServer::handle_delete(AsyncWebServerRequest *request) {
  if (!this->deletion_enabled_) {
    request->send(403);
    return;
  }

  std::string path = this->extract_path_from_url(request->url().c_str());
  std::string absolute_path = this->build_absolute_path(path);

  if (this->sd_mmc_card_->remove(absolute_path)) {
    request->send(200);
  } else {
    request->send(500);
  }
}

void SDFileServer::handle_download(AsyncWebServerRequest *request, std::string const &path) const {
  if (!this->download_enabled_) {
    request->send(403);
    return;
  }

  std::string absolute_path = this->build_absolute_path(path);
  auto file = this->sd_mmc_card_->open(absolute_path, "r");

  if (!file) {
    request->send(404);
    return;
  }

  request->send(this->sd_mmc_card_, absolute_path.c_str(), "application/octet-stream");
}

}  // namespace sd_file_server
}  // namespace esphome





