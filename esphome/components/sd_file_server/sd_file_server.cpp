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
  ESP_LOGD(TAG, "can handle %s %u", request->url().c_str(),
           str_startswith(std::string(request->url().c_str()), this->build_prefix()));
  return str_startswith(std::string(request->url().c_str()), this->build_prefix());
}

void SDFileServer::handleRequest(AsyncWebServerRequest *request) {
  ESP_LOGD(TAG, "%s", request->url().c_str());
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
  auto response = request->beginResponseStream("text/html");
  response->print("<!DOCTYPE html><html><head>");
  response->print("<title>SD Card Files</title>");
  // Ajout des styles CSS ici
  response->print("<style>");
  response->print("body { font-family: Arial, sans-serif; background-color: #f4f4f4; color: #333; }");
  response->print(".container { width: 80%; margin: auto; overflow: hidden; }");
  response->print("table { width: 100%; border-collapse: collapse; margin-top: 20px; }");
  response->print("th, td { padding: 12px 15px; text-align: left; border-bottom: 1px solid #ddd; }");
  response->print("th { background-color: #007bff; color: white; }");
  response->print("tr:hover { background-color: #f5f5f5; }");
  response->print(".actions a { display: inline-block; padding: 8px 12px; margin: 5px; background-color: #007bff; color: white; text-decoration: none; border-radius: 5px; transition: background-color 0.3s ease; }");
  response->print(".actions a:hover { background-color: #0056b3; }");
  response->print("</style>");
  response->print("</head><body>");
  response->printf("<div class='container'><h1>Files in %s</h1>", path.c_str());
  response->print("<table>");
  response->print("<thead><tr><th>Name</th><th>Actions</th></tr></thead><tbody>");

  std::vector<sd_mmc_card::FileInfo> file_list;
  this->sd_mmc_card_->list_dir(path, file_list);
  for (auto &file : file_list) {
    this->write_row(response, file);
  }

  response->print("</tbody></table>");
    // Ajout du script JavaScript ici
  response->print("<script>");
  response->print("document.querySelectorAll('.delete-link').forEach(link => {");
  response->print("  link.addEventListener('click', function(event) {");
  response->print("    event.preventDefault();");
  response->print("    if (confirm('Are you sure you want to delete this file?')) {");
  response->print("      window.location.href = this.href;");
  response->print("    }");
  response->print("  });");
  response->print("});");
  response->print("</script>");
  response->print("</div></body></html>");
  request->send(response);
}

void SDFileServer::write_row(AsyncResponseStream *response, sd_mmc_card::FileInfo const &info) const {
  std::string uri = "/" + Path::join(this->url_prefix_, Path::remove_root_path(info.path, this->root_path_));
  std::string file_name = Path::file_name(info.path);

  response->print("<tr>");
  response->printf("<td>%s</td>", file_name.c_str());
  response->print("<td class='actions'>");
  if (this->download_enabled_) {
    response->printf("<a href='%s' download>Download</a>", uri.c_str());
  }
  if (this->deletion_enabled_) {
    response->printf("<a href='%s' class='delete-link'>Delete</a>", uri.c_str());
  }
  response->print("</td>");
  response->print("</tr>");
}

void SDFileServer::handle_delete(AsyncWebServerRequest *request) {
  if (!this->deletion_enabled_) {
    request->send(401, "application/json", "{ \"error\": \"deletion is disabled\" }");
    return;
  }
  std::string extracted = this->extract_path_from_url(std::string(request->url().c_str()));
  std::string path = this->build_absolute_path(extracted);
  ESP_LOGD(TAG, "deleting file %s", path.c_str());

  if (this->sd_mmc_card_->is_directory(path)) {
    request->send(400, "application/json", "{ \"error\": \"can't delete a directory\" }");
    return;
  }

  if (!this->sd_mmc_card_->delete_file(path)) {
    request->send(500, "application/json", "{ \"error\": \"deleting file failed\" }");
    return;
  }

  auto response = request->beginResponse(200, "application/json", "{ \"message\": \"file deleted\" }");
  response->addHeader("Connection", "close");
  request->send(response);
}

void SDFileServer::handle_download(AsyncWebServerRequest *request, std::string const &path) const {
  if (!this->download_enabled_) {
    request->send(401, "application/json", "{ \"error\": \"download is disabled\" }");
    return;
  }

  ESP_LOGD(TAG, "downloading file %s", path.c_str());

  if (this->sd_mmc_card_->is_directory(path)) {
    request->send(400, "application/json", "{ \"error\": \"can't download a directory\" }");
    return;
  }

  if (!this->sd_mmc_card_->file_exists(path)) {
    request->send(404, "application/json", "{ \"error\": \"file not found\" }");
    return;
  }

  AsyncWebServerResponse *response = request->beginResponse(this->sd_mmc_card_->get_file_size(path), "application/octet-stream",
                                                            [this, path](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
    return this->sd_mmc_card_->read_file(path, buffer, maxLen, index);
  });
  response->setContentLength(this->sd_mmc_card_->get_file_size(path));
  request->send(response);
}

std::string SDFileServer::build_prefix() const { return "/" + this->url_prefix_; }

std::string SDFileServer::extract_path_from_url(std::string const &url) const {
  std::string prefix = this->build_prefix();
  if (url.size() < prefix.size())
    return "";
  return url.substr(prefix.size());
}

std::string SDFileServer::build_absolute_path(std::string path) const {
  if (Path::is_absolute(path))
    return path;

  return Path::join(this->root_path_, path);
}

std::string Path::file_name(std::string const &path) {
  size_t pos = path.rfind(Path::separator);
  if (pos == std::string::npos) {
    return path;
  }
  return path.substr(pos + 1);
}

bool Path::is_absolute(std::string const &path) { return !path.empty() && path[0] == Path::separator; }

bool Path::trailing_slash(std::string const &path) { return !path.empty() && path[path.size() - 1] == Path::separator; }

std::string Path::join(std::string const &a, std::string const &b) {
  if (a.empty())
    return b;
  if (b.empty())
    return a;

  bool a_trail = Path::trailing_slash(a);
  bool b_abs = Path::is_absolute(b);

  if (a_trail && b_abs) {
    return a.substr(0, a.length() - 1) + b;
  }
  if (!a_trail && !b_abs) {
    return a + Path::separator + b;
  }

  return a + b;
}

std::string Path::remove_root_path(std::string path, std::string const &root) {
    if (str_startswith(path, root)) {
        return path.substr(root.length());
    }
    return path;
}

}  // namespace sd_file_server
}  // namespace esphome








