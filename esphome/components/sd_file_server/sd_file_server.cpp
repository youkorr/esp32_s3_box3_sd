#include "sd_file_server.h"
#include "esphome/core/log.h"
#include "esphome/components/network/util.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace sd_file_server {

SDFileServer::SDFileServer(web_server_base::WebServerBase *base) : base_(base) {
  this->deletion_enabled_ = false;
  this->download_enabled_ = true;
  this->upload_enabled_ = true;
}

void SDFileServer::set_url_prefix(std::string const &url_prefix) {
  this->url_prefix_ = url_prefix;
}

void SDFileServer::set_root_path(std::string const &root_path) {
  this->root_path_ = root_path;
}

void SDFileServer::set_sd_mmc_card(sd_mmc_card::SdMmc *sd_mmc_card) {
  this->sd_mmc_card_ = sd_mmc_card;
}

void SDFileServer::set_deletion_enabled(bool deletion_enabled) {
  this->deletion_enabled_ = deletion_enabled;
}

void SDFileServer::set_download_enabled(bool download_enabled) {
  this->download_enabled_ = download_enabled;
}

void SDFileServer::set_upload_enabled(bool upload_enabled) {
  this->upload_enabled_ = upload_enabled;
}

std::string SDFileServer::build_prefix() const {
  return this->url_prefix_ + "/";
}

std::string SDFileServer::extract_path_from_url(std::string const &url) const {
  if (url.rfind(this->url_prefix_, 0) != 0)
    return "";

  return url.substr(this->url_prefix_.length());
}

std::string SDFileServer::build_absolute_path(std::string path) const {
  if (path.empty() || path == "/")
    return this->root_path_;

  if (path[0] == '/')
    path = path.substr(1);

  return Path::join(this->root_path_, path);
}

void SDFileServer::write_row(AsyncResponseStream *response, sd_mmc_card::FileInfo const &info) const {
  response->print("<tr>");
  response->printf("<td>%s</td>", info.name.c_str());
  response->printf("<td>%d</td>", info.size);
  response->print("</tr>");
}

void SDFileServer::handle_index(AsyncWebServerRequest *request, std::string const &path) const {
  auto *response = request->beginResponseStream("text/html");
  response->print("<html><head><title>SD File Server</title></head><body>");
  response->print("<h1>SD Card Contents</h1>");
  response->print("<table border='1'>");
  response->print("<tr><th>Name</th><th>Size</th></tr>");

  auto files = this->sd_mmc_card_->list_files(path);
  for (auto const &file : files) {
    this->write_row(response, file);
  }

  response->print("</table></body></html>");
  request->send(response);
}

void SDFileServer::handle_get(AsyncWebServerRequest *request) const {
  std::string path = this->extract_path_from_url(request->url().c_str());
  std::string absolute_path = this->build_absolute_path(path);

  if (this->sd_mmc_card_->is_directory(absolute_path)) {
    this->handle_index(request, absolute_path);
    return;
  }

  this->handle_download(request, path);
}

void SDFileServer::handleRequest(AsyncWebServerRequest *request) {
  if (request->method() == HTTP_GET) {
    this->handle_get(request);
  } else if (request->method() == HTTP_DELETE) {
    this->handle_delete(request);
  } else {
    request->send(405);
  }
}

std::string Path::file_name(std::string const &path) {
  size_t pos = path.find_last_of(separator);
  if (pos == std::string::npos)
    return path;
  return path.substr(pos + 1);
}

bool Path::is_absolute(std::string const &path) {
  return !path.empty() && path[0] == separator;
}

bool Path::trailing_slash(std::string const &path) {
  return !path.empty() && path.back() == separator;
}

std::string Path::join(std::string const &path1, std::string const &path2) {
  if (path1.empty())
    return path2;
  if (path2.empty())
    return path1;

  std::string result = path1;
  if (result.back() != separator)
    result += separator;
  if (path2.front() == separator)
    result += path2.substr(1);
  else
    result += path2;

  return result;
}

std::string Path::remove_root_path(std::string path, std::string const &root) {
  if (path.rfind(root, 0) == 0)
    return path;

  return path.substr(root.length());
}

}  // namespace sd_file_server
}  // namespace esphome





