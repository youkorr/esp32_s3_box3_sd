#include "sd_file_server.h"
#include "esphome/core/log.h"
#include "esphome/components/network/util.h"
#include "esphome/core/helpers.h"
#include <algorithm>
#include <cstdio>
#include <sys/stat.h>

namespace esphome {
namespace sd_file_server {

static const char *TAG = "sd_file_server";

SDFileServer::SDFileServer(web_server_base::WebServerBase *base) : base_(base) {}

void SDFileServer::setup() { 
  this->base_->add_handler(this); 
  // Configurer explicitement pour gérer les uploads de fichiers
  this->base_->get_server()->onFileUpload([this](AsyncWebServerRequest *request, const String &filename, size_t index,
                                               uint8_t *data, size_t len, bool final) {
    this->handleUpload(request, filename, index, data, len, final);
  });
}

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
    std::string extracted = this->extract_path_from_url(std::string(request->url().c_str()));
    std::string path = this->build_absolute_path(extracted);
    
    ESP_LOGD(TAG, "Handling request: %s, Method: %d, Path: %s", 
             request->url().c_str(), request->method(), path.c_str());

    if (request->method() == HTTP_GET) {
      this->handle_get(request);
      return;
    }
    if (request->method() == HTTP_POST && this->upload_enabled_) {
      // Le traitement principal de l'upload se fait dans handleUpload
      // On accepte juste la requête POST initiale ici
      request->send(200);
      return;
    }
    if (request->method() == HTTP_DELETE && this->deletion_enabled_) {
      this->handle_delete(request, path);
      return;
    }
    
    // Si on arrive ici, la méthode n'est pas supportée
    request->send(405, "text/plain", "Method Not Allowed");
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
  std::string file_name(filename.c_str());
  std::string full_path = Path::join(path, file_name);

  ESP_LOGD(TAG, "Upload request: %s, File: %s, Index: %u, Length: %u, Final: %d", 
           path.c_str(), file_name.c_str(), index, len, final);

  if (index == 0 && !this->sd_mmc_card_->is_directory(path)) {
    ESP_LOGE(TAG, "Upload failed: destination %s is not a directory", path.c_str());
    auto response = request->beginResponse(401, "application/json", "{ \"error\": \"invalid upload folder\" }");
    response->addHeader("Connection", "close");
    request->send(response);
    return;
  }

  // Pour le premier segment
  if (index == 0) {
    ESP_LOGD(TAG, "Starting upload of file %s to %s", file_name.c_str(), path.c_str());
    // Supprimer le fichier s'il existe déjà pour éviter les conflits
    this->sd_mmc_card_->delete_file(full_path);
    if (!this->sd_mmc_card_->write_file(full_path.c_str(), data, len)) {
      ESP_LOGE(TAG, "Failed to write initial segment to %s", full_path.c_str());
    }
    return;
  }

  // Pour les segments suivants
  if (!this->sd_mmc_card_->append_file(full_path.c_str(), data, len)) {
    ESP_LOGE(TAG, "Failed to append segment to %s", full_path.c_str());
  }

  // Pour le segment final
  if (final) {
    ESP_LOGD(TAG, "Completed upload of file %s, total size: %u", file_name.c_str(), index + len);
    auto response = request->beginResponse(201, "application/json", "{ \"message\": \"upload success\" }");
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

void SDFileServer::handle_get(AsyncWebServerRequest *request) const {
  std::string extracted = this->extract_path_from_url(std::string(request->url().c_str()));
  std::string path = this->build_absolute_path(extracted);

  if (!this->sd_mmc_card_->is_directory(path)) {
    handle_download(request, path);
    return;
  }

  handle_index(request, path);
}

std::string format_file_size(size_t size) {
  const char* units[] = {"B", "KB", "MB", "GB"};
  size_t unit_index = 0;
  double size_d = static_cast<double>(size);

  while (size_d >= 1024 && unit_index < 3) {
    size_d /= 1024;
    unit_index++;
  }

  char buffer[32];
  snprintf(buffer, sizeof(buffer), "%.2f %s", size_d, units[unit_index]);
  return std::string(buffer);
}

std::string get_file_extension(const std::string& filename) {
  size_t dot_pos = filename.rfind('.');
  if (dot_pos == std::string::npos)
    return "File";
  
  std::string ext = filename.substr(dot_pos + 1);
  std::transform(ext.begin(), ext.end(), ext.begin(), ::toupper);
  return ext + " File";
}

void SDFileServer::write_row(AsyncResponseStream *response, sd_mmc_card::FileInfo const &info) const {
  std::string uri = "/" + Path::join(this->url_prefix_, Path::remove_root_path(info.path, this->root_path_));
  std::string file_name = Path::file_name(info.path);
  std::string file_size = info.is_directory ? "-" : format_file_size(info.size);
  std::string file_type = info.is_directory ? "Folder" : get_file_extension(file_name);

  response->print("<tr class=\"file-row\"><td>");
  if (info.is_directory) {
    response->print("<a href=\"");
    response->print(uri.c_str());
    response->print("\" class=\"folder-link\">");
    response->print(file_name.c_str());
    response->print("</a>");
  } else {
    response->print("<span class=\"file-name\">");
    response->print(file_name.c_str());
    response->print("</span>");
  }
  response->print("</td><td>");
  response->print(file_type.c_str());
  response->print("</td><td>");
  response->print(file_size.c_str());
  response->print("</td><td class=\"actions\">");
  if (!info.is_directory) {
    if (this->download_enabled_) {
      response->print("<button class=\"btn download-btn\" onClick=\"download_file('");
      response->print(uri.c_str());
      response->print("','");
      response->print(file_name.c_str());
      response->print("')\">Download</button>");
    }
    if (this->deletion_enabled_) {
      response->print("<button class=\"btn delete-btn\" onClick=\"delete_file('");
      response->print(uri.c_str());
      response->print("')\">Delete</button>");
    }
  }
  response->print("</td></tr>");
}

void SDFileServer::handle_index(AsyncWebServerRequest *request, std::string const &path) const {
  AsyncResponseStream *response = request->beginResponseStream("text/html");
  response->print(F("<!DOCTYPE html><html lang=\"en\"><head><meta charset=UTF-8><meta "
                    "name=viewport content=\"width=device-width, initial-scale=1,user-scalable=no\">"
                    "<style>"
                    "body { font-family: 'Segoe UI', sans-serif; background: #f5f5f5; color: #333; margin: 0; padding: 20px; }"
                    "h1 { color: #2c3e50; margin-bottom: 20px; }"
                    "h2 { color: #666; margin-bottom: 15px; }"
                    "table { width: 100%; border-collapse: collapse; background: white; box-shadow: 0 2px 4px rgba(0,0,0,0.1); border-radius: 8px; overflow: hidden; }"
                    "th, td { padding: 12px 15px; text-align: left; border-bottom: 1px solid #eee; }"
                    "th { background: #2c3e50; color: white; font-weight: 600; }"
                    "tr.file-row:hover { background: #f9f9f9; }"
                    "tr.file-row:hover .actions { opacity: 1; }"
                    ".folder-link { color: #2980b9; text-decoration: none; font-weight: 500; }"
                    ".folder-link:hover { text-decoration: underline; }"
                    ".file-name { font-weight: 500; }"
                    ".actions { opacity: 0; transition: opacity 0.2s; }"
                    ".btn { border: none; padding: 6px 12px; border-radius: 4px; cursor: pointer; font-size: 14px; transition: all 0.2s; }"
                    ".download-btn { background: #27ae60; color: white; margin-right: 8px; }"
                    ".download-btn:hover { background: #219653; }"
                    ".delete-btn { background: #e74c3c; color: white; }"
                    ".delete-btn:hover { background: #c0392b; }"
                    ".upload-form { margin: 20px 0; padding: 15px; background: white; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }"
                    ".upload-form input[type='file'] { margin-right: 10px; }"
                    ".upload-form input[type='submit'] { background: #2980b9; color: white; border: none; padding: 8px 16px; border-radius: 4px; cursor: pointer; }"
                    ".upload-form input[type='submit']:hover { background: #2472a4; }"
                    "</style>"
                    "</head><body>"
                    "<h1>SD Card Content</h1><h2>Folder "));

  response->print(path.c_str());
  response->print(F("</h2>"));
  if (this->upload_enabled_) {
    response->print(F("<div class=\"upload-form\">"
                      "<form method=\"POST\" enctype=\"multipart/form-data\">"
                      "<input type=\"file\" name=\"file\">"
                      "<input type=\"submit\" value=\"Upload File\">"
                      "</form>"
                      "</div>"));
  }
  response->print(F("<a href=\"/"));
  response->print(this->url_prefix_.c_str());
  response->print(F("\" class=\"home-link\">← Back to Home</a>"
                    "<table><thead><tr><th>Name<th>Type<th>Size<th>Actions<tbody>"));

  auto entries = this->sd_mmc_card_->list_directory_file_info(path, 0);
  for (auto const &entry : entries)
    write_row(response, entry);

  response->print(F("</tbody></table>"
                    "<script>"
                    "function delete_file(path) {"
                    "if (confirm('Are you sure you want to delete this file?')) {"
                    "fetch(path, {method: 'DELETE'}).then(response => {"
                    "if(response.ok) { location.reload(); }"
                    "else { alert('Delete failed: ' + response.statusText); }"
                    "}).catch(err => { alert('Error: ' + err); });"
                    "}}"
                    "function download_file(path, filename) {"
                    "const a = document.createElement('a');"
                    "a.href = path;"
                    "a.download = filename;"
                    "document.body.appendChild(a);"
                    "a.click();"
                    "document.body.removeChild(a);"
                    "}"
                    "</script>"
                    "</body></html>"));

  request->send(response);
}

void SDFileServer::handle_download(AsyncWebServerRequest *request, std::string const &path) const {
  if (!this->download_enabled_) {
    request->send(401, "application/json", "{ \"error\": \"file download is disabled\" }");
    return;
  }

  ESP_LOGD(TAG, "Processing download request for: %s", path.c_str());

#ifdef USE_ESP_IDF
  // Implémentation améliorée pour ESP-IDF
  struct stat st;
  if (stat(path.c_str(), &st) != 0) {
    ESP_LOGE(TAG, "File stat failed for: %s", path.c_str());
    request->send(404, "text/plain", "File not found");
    return;
  }
  
  FILE* f = fopen(path.c_str(), "rb");
  if (!f) {
    ESP_LOGE(TAG, "Failed to open file: %s", path.c_str());
    request->send(404, "text/plain", "File not found");
    return;
  }

  size_t file_size = st.st_size;
  ESP_LOGD(TAG, "File size: %zu bytes", file_size);
  
  std::string filename = Path::file_name(path);
  
  // Utiliser directement le callback pour envoyer le fichier en streaming
  AsyncWebServerResponse *response = request->beginResponse("application/octet-stream", file_size,
      [f, file_size](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
        if (index >= file_size) {
          return 0;  // Fin du fichier
        }
        
        // Déterminer combien d'octets lire
        size_t bytesToRead = std::min(maxLen, file_size - index);
        
        // Positionner le curseur du fichier
        fseek(f, index, SEEK_SET);
        
        // Lire les données
        size_t bytesRead = fread(buffer, 1, bytesToRead, f);
        
        if (bytesRead == 0 && bytesToRead > 0) {
          ESP_LOGE(TAG, "Read error at position %zu", index);
        }
        
        return bytesRead;
      },
      [f](void) {
        // Callback de nettoyage
        fclose(f);
        ESP_LOGD(TAG, "File download completed, file closed");
      }
  );
  
  response->addHeader("Content-Disposition", ("attachment; filename=" + filename).c_str());
  request->send(response);
#else
  // Version non-ESP-IDF simplifiée et améliorée
  ESP_LOGD(TAG, "Reading file with sd_mmc_card_->read_file");
  auto file = this->sd_mmc_card_->read_file(path);
  if (file.size() == 0) {
    ESP_LOGE(TAG, "Failed to read file or file is empty");
    request->send(401, "application/json", "{ \"error\": \"failed to read file\" }");
    return;
  }
  
  std::string filename = Path::file_name(path);
  size_t file_size = file.size();
  
  ESP_LOGD(TAG, "File read successfully, size: %zu bytes", file_size);
  
  AsyncWebServerResponse *response = request->beginResponse("application/octet-stream", file_size,
      [file = std::move(file)](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
        size_t remaining = file.size() - index;
        if (remaining == 0) return 0;
        
        size_t to_copy = std::min(maxLen, remaining);
        memcpy(buffer, file.data() + index, to_copy);
        return to_copy;
      }
  );
  
  response->addHeader("Content-Disposition", "attachment; filename=" + filename);
  request->send(response);
#endif
}

void SDFileServer::handle_delete(AsyncWebServerRequest *request, const std::string& path) const {
  if (!this->deletion_enabled_) {
    request->send(401, "application/json", "{ \"error\": \"deletion is disabled\" }");
    return;
  }
  
  ESP_LOGD(TAG, "Processing delete request for: %s", path.c_str());

#ifdef USE_ESP_IDF
  struct stat st;
  if (stat(path.c_str(), &st) != 0) {
    ESP_LOGE(TAG, "File stat failed for: %s", path.c_str());
    request->send(404, "application/json", "{ \"error\": \"file not found\" }");
    return;
  }

  if (S_ISDIR(st.st_mode)) {
    ESP_LOGE(TAG, "Cannot delete directory: %s", path.c_str());
    request->send(400, "application/json", "{ \"error\": \"cannot delete directories\" }");
    return;
  }

  if (unlink(path.c_str()) != 0) {
    ESP_LOGE(TAG, "Deletion failed for: %s", path.c_str());
    request->send(500, "application/json", "{ \"error\": \"deletion failed\" }");
    return;
  }

  ESP_LOGD(TAG, "File deleted successfully: %s", path.c_str());
#else
  // Vérifier si c'est un répertoire
  if (this->sd_mmc_card_->is_directory(path)) {
    ESP_LOGE(TAG, "Cannot delete directory: %s", path.c_str());
    request->send(400, "application/json", "{ \"error\": \"cannot delete directories\" }");
    return;
  }
  
  // Vérifier si le fichier existe
  auto fileCheck = this->sd_mmc_card_->read_file(path);
  if (fileCheck.size() == 0) {
    ESP_LOGE(TAG, "File not found: %s", path.c_str());
    request->send(404, "application/json", "{ \"error\": \"file not found\" }");
    return;
  }
  
  // Tenter de supprimer le fichier
  if (!this->sd_mmc_card_->delete_file(path)) {
    ESP_LOGE(TAG, "Deletion failed for: %s", path.c_str());
    request->send(500, "application/json", "{ \"error\": \"deletion failed\" }");
    return;
  }
  
  ESP_LOGD(TAG, "File deleted successfully: %s", path.c_str());
#endif

  request->send(200, "application/json", "{ \"message\": \"file deleted\" }");
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

bool Path::is_absolute(std::string const &path) { 
  return path.size() && path[0] == separator; 
}

bool Path::trailing_slash(std::string const &path) { 
  return path.size() && path[path.length() - 1] == separator; 
}

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






























