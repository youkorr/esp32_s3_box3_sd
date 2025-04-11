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
  ESP_LOGCONFIG(TAG, "  Deletation Enabled: %s", TRUEFALSE(this->deletion_enabled_));
  ESP_LOGCONFIG(TAG, "  Download Enabled : %s", TRUEFALSE(this->download_enabled_));
  ESP_LOGCONFIG(TAG, "  Upload Enabled : %s", TRUEFALSE(this->upload_enabled_));
}

bool SDFileServer::canHandle(AsyncWebServerRequest *request) {
  if (request == nullptr || request->url().length() == 0) {
    return false;
  }
  
  std::string url = request->url().c_str();
  bool can_handle = str_startswith(url, this->build_prefix());
  ESP_LOGD(TAG, "can handle %s %u", url.c_str(), can_handle);
  return can_handle;
}

void SDFileServer::handleRequest(AsyncWebServerRequest *request) {
  try {
    ESP_LOGD(TAG, "Handling request: %s", request->url().c_str());
    if (str_startswith(std::string(request->url().c_str()), this->build_prefix())) {
      if (request->method() == HTTP_GET) {
        this->handle_get(request);
        return;
      }
      if (request->method() == HTTP_DELETE) {
        this->handle_delete(request);
        return;
      }
      // Gérer les méthodes non supportées
      request->send(405, "application/json", "{ \"error\": \"method not allowed\" }");
    }
  } catch (std::exception &e) {
    ESP_LOGE(TAG, "Request handling error: %s", e.what());
    request->send(500, "application/json", "{ \"error\": \"internal server error\" }");
  }
}

void SDFileServer::handleUpload(AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data,
                                size_t len, bool final) {
  if (!this->upload_enabled_) {
    request->send(401, "application/json", "{ \"error\": \"file upload is disabled\" }");
    return;
  }
  
  try {
    std::string extracted = this->extract_path_from_url(std::string(request->url().c_str()));
    std::string path = this->build_absolute_path(extracted);

    // Vérifier si le répertoire existe
    if (index == 0 && !this->sd_mmc_card_->is_directory(path)) {
      auto response = request->beginResponse(404, "application/json", "{ \"error\": \"upload directory not found\" }");
      response->addHeader("Connection", "close");
      request->send(response);
      return;
    }
    
    // Valider le nom de fichier
    std::string file_name(filename.c_str());
    if (file_name.empty() || file_name.find("/") != std::string::npos || file_name.find("\\") != std::string::npos) {
      auto response = request->beginResponse(400, "application/json", "{ \"error\": \"invalid filename\" }");
      response->addHeader("Connection", "close");
      request->send(response);
      return;
    }
    
    std::string full_path = Path::join(path, file_name);
    
    if (index == 0) {
      ESP_LOGD(TAG, "uploading file %s to %s", file_name.c_str(), path.c_str());
      // Créer un nouveau fichier
      if (!this->sd_mmc_card_->write_file(full_path.c_str(), data, len)) {
        auto response = request->beginResponse(500, "application/json", "{ \"error\": \"failed to create file\" }");
        response->addHeader("Connection", "close");
        request->send(response);
        return;
      }
    } else {
      // Ajouter au fichier existant
      if (!this->sd_mmc_card_->append_file(full_path.c_str(), data, len)) {
        auto response = request->beginResponse(500, "application/json", "{ \"error\": \"failed to append to file\" }");
        response->addHeader("Connection", "close");
        request->send(response);
        return;
      }
    }
    
    if (final) {
      auto response = request->beginResponse(201, "application/json", "{ \"success\": true, \"file\": \"" + file_name + "\" }");
      response->addHeader("Connection", "close");
      request->send(response);
    }
  } catch (std::exception &e) {
    ESP_LOGE(TAG, "Upload error: %s", e.what());
    auto response = request->beginResponse(500, "application/json", "{ \"error\": \"internal server error\" }");
    response->addHeader("Connection", "close");
    request->send(response);
  }
}

void SDFileServer::set_url_prefix(std::string const &prefix) { this->url_prefix_ = prefix; }

void SDFileServer::set_root_path(std::string const &path) { this->root_path_ = path; }

void SDFileServer::set_sd_mmc_card(sd_mmc_card::SdMmc *card) { this->sd_mmc_card_ = card; }

void SDFileServer::set_deletion_enabled(bool allow) { this->deletion_enabled_ = allow; }

void SDFileServer::set_download_enabled(bool allow) { this->download_enabled_ = allow; }

void SDFileServer::set_upload_enabled(bool allow) { this->upload_enabled_ = allow; }

bool SDFileServer::file_exists(const std::string &path) const {
  return this->sd_mmc_card_->is_file(path);
}

void SDFileServer::handle_get(AsyncWebServerRequest *request) const {
  try {
    std::string extracted = this->extract_path_from_url(std::string(request->url().c_str()));
    std::string path = this->build_absolute_path(extracted);

    if (!this->sd_mmc_card_->exists(path)) {
      request->send(404, "application/json", "{ \"error\": \"path not found\" }");
      return;
    }

    if (!this->sd_mmc_card_->is_directory(path)) {
      handle_download(request, path);
      return;
    }

    handle_index(request, path);
  } catch (std::exception &e) {
    ESP_LOGE(TAG, "GET handling error: %s", e.what());
    request->send(500, "application/json", "{ \"error\": \"internal server error\" }");
  }
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
  response->print("</td></tr>");
}

void SDFileServer::handle_index(AsyncWebServerRequest *request, std::string const &path) const {
  try {
    AsyncResponseStream *response = request->beginResponseStream("text/html");
    response->print(F("<!DOCTYPE html><html lang=\"en\"><head><meta charset=UTF-8><meta "
                      "name=viewport content=\"width=device-width, initial-scale=1,user-scalable=no\">"
                      "<style>body{font-family:sans-serif}table{border-collapse:collapse;width:100%}"
                      "th,td{border:1px solid #ddd;padding:8px}tr:nth-child(even){background-color:#f2f2f2}"
                      "th{padding-top:12px;padding-bottom:12px;text-align:left;background-color:#4CAF50;color:white}"
                      "button{margin:2px;padding:5px;background-color:#4CAF50;color:white;border:none;cursor:pointer}"
                      "button:hover{background-color:#45a049}</style>"
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
    for (auto const &entry : entries)
      write_row(response, entry);

    response->print(F("</tbody></table>"
                      "<script>"
                      "function delete_file(path) {"
                      "  if (confirm('Are you sure you want to delete this file?')) {"
                      "    fetch(path, {method: \"DELETE\"})"
                      "      .then(response => response.json())"
                      "      .then(data => {"
                      "        if (data.success) {"
                      "          alert('File deleted successfully');"
                      "          location.reload();"
                      "        } else {"
                      "          alert('Error: ' + (data.error || 'Failed to delete file'));"
                      "        }"
                      "      })"
                      "      .catch(error => {"
                      "        console.error('Error:', error);"
                      "        alert('Error deleting file');"
                      "      });"
                      "  }"
                      "}"
                      "function download_file(path, filename) {"
                      "  fetch(path)"
                      "    .then(response => {"
                      "      if (!response.ok) {"
                      "        throw new Error('Network response was not ok');"
                      "      }"
                      "      return response.blob();"
                      "    })"
                      "    .then(blob => {"
                      "      const link = document.createElement('a');"
                      "      link.href = URL.createObjectURL(blob);"
                      "      link.download = filename;"
                      "      document.body.appendChild(link);"
                      "      link.click();"
                      "      document.body.removeChild(link);"
                      "    })"
                      "    .catch(error => {"
                      "      console.error('Download error:', error);"
                      "      alert('Error downloading file: ' + error.message);"
                      "    });"
                      "}"
                      "</script>"
                      "</body></html>"));

    request->send(response);
  } catch (std::exception &e) {
    ESP_LOGE(TAG, "Index handling error: %s", e.what());
    request->send(500, "application/json", "{ \"error\": \"internal server error\" }");
  }
}

void SDFileServer::handle_download(AsyncWebServerRequest *request, std::string const &path) const {
  if (!this->download_enabled_) {
    request->send(401, "application/json", "{ \"error\": \"file download is disabled\" }");
    return;
  }

  try {
    // Vérifier si le fichier existe
    if (!this->sd_mmc_card_->exists(path) || this->sd_mmc_card_->is_directory(path)) {
      request->send(404, "application/json", "{ \"error\": \"file not found\" }");
      return;
    }

    std::vector<uint8_t> file;
    try {
      file = this->sd_mmc_card_->read_file(path);
    } catch (std::exception &e) {
      ESP_LOGE(TAG, "Error reading file: %s", e.what());
      request->send(500, "application/json", "{ \"error\": \"failed to read file\" }");
      return;
    }
    
    if (file.size() == 0) {
      request->send(404, "application/json", "{ \"error\": \"empty or unreadable file\" }");
      return;
    }
    
    std::string content_type = "application/octet-stream";
    // Déterminer le type MIME si possible
    std::string extension = "";
    size_t pos = path.find_last_of(".");
    if (pos != std::string::npos) {
      extension = path.substr(pos + 1);
      if (extension == "jpg" || extension == "jpeg") content_type = "image/jpeg";
      else if (extension == "png") content_type = "image/png";
      else if (extension == "txt") content_type = "text/plain";
      else if (extension == "html" || extension == "htm") content_type = "text/html";
      else if (extension == "css") content_type = "text/css";
      else if (extension == "js") content_type = "application/javascript";
      else if (extension == "json") content_type = "application/json";
      else if (extension == "pdf") content_type = "application/pdf";
    }
    
    AsyncWebServerResponse *response = nullptr;
    
#ifdef USE_ESP_IDF
    response = request->beginResponse_P(200, content_type.c_str(), file.data(), file.size());
#else
    response = request->beginResponseStream(content_type.c_str(), file.size());
    response->write(file.data(), file.size());
#endif
    
    if (response != nullptr) {
      std::string filename = Path::file_name(path);
      response->addHeader("Content-Disposition", ("attachment; filename=\"" + filename + "\"").c_str());
      response->addHeader("Cache-Control", "no-cache");
      
      request->send(response);
    } else {
      request->send(500, "application/json", "{ \"error\": \"failed to create response\" }");
    }
  } catch (std::exception &e) {
    ESP_LOGE(TAG, "Download error: %s", e.what());
    request->send(500, "application/json", "{ \"error\": \"internal server error\" }");
  }
}

void SDFileServer::handle_delete(AsyncWebServerRequest *request) {
  if (!this->deletion_enabled_) {
    request->send(401, "application/json", "{ \"error\": \"file deletion is disabled\" }");
    return;
  }
  
  try {
    std::string extracted = this->extract_path_from_url(std::string(request->url().c_str()));
    std::string path = this->build_absolute_path(extracted);
    
    // Vérifier si le fichier existe
    if (!this->sd_mmc_card_->exists(path)) {
      request->send(404, "application/json", "{ \"error\": \"file not found\" }");
      return;
    }
    
    if (this->sd_mmc_card_->is_directory(path)) {
      request->send(400, "application/json", "{ \"error\": \"cannot delete a directory\" }");
      return;
    }
    
    if (this->sd_mmc_card_->delete_file(path)) {
      request->send(200, "application/json", "{ \"success\": true, \"message\": \"file deleted successfully\" }");
      return;
    }
    
    request->send(500, "application/json", "{ \"error\": \"failed to delete file\" }");
  } catch (std::exception &e) {
    ESP_LOGE(TAG, "Delete error: %s", e.what());
    request->send(500, "application/json", "{ \"error\": \"internal server error\" }");
  }
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
