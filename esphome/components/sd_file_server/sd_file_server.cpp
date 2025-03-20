#include "sd_file_server.h"
#include "esphome/core/log.h"
#include "esphome/components/network/util.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace sd_file_server {

static const char *TAG = "sd_file_server";

// Variables statiques pour l'upload
static std::string upload_path;
static FILE *upload_file = nullptr;
static size_t total_size = 0;
static bool use_chunked_mode = false;
static uint8_t current_chunk[16 * 1024]; // Tableau statique pour éviter les allocations dynamiques
static size_t chunk_size = 0;

// Constructeurs
SDFileServer::SDFileServer(web_server_base::WebServerBase *base) : base_(base) {}
#ifdef USE_ESP_IDF
SDFileServer::SDFileServer(httpd_handle_t server) : server_(server) {}
#endif

void SDFileServer::setup() {
#ifdef USE_ESP_IDF
    // Configuration des gestionnaires de requêtes ESP-IDF
    httpd_uri_t file_get = {
        .uri = this->build_prefix().c_str(),
        .method = HTTP_GET,
        .handler = SDFileServer::handle_get_static,
        .user_ctx = this
    };
    httpd_uri_t file_delete = {
        .uri = this->build_prefix().c_str(),
        .method = HTTP_DELETE,
        .handler = SDFileServer::handle_delete_static,
        .user_ctx = this
    };
    httpd_uri_t file_post = {
        .uri = this->build_prefix().c_str(),
        .method = HTTP_POST,
        .handler = SDFileServer::handle_post_static,
        .user_ctx = this
    };
    httpd_register_uri_handler(this->server_, &file_get);
    httpd_register_uri_handler(this->server_, &file_delete);
    httpd_register_uri_handler(this->server_, &file_post);
#else
    this->base_->add_handler(this);
#endif
}

void SDFileServer::dump_config() {
    ESP_LOGCONFIG(TAG, "SD File Server:");
#ifdef USE_ESP_IDF
    ESP_LOGCONFIG(TAG, "  Address: %s", network::get_use_address().c_str());
#else
    ESP_LOGCONFIG(TAG, "  Address: %s:%u", network::get_use_address().c_str(), this->base_->get_port());
#endif
    ESP_LOGCONFIG(TAG, "  Url Prefix: %s", this->url_prefix_.c_str());
    ESP_LOGCONFIG(TAG, "  Root Path: %s", this->root_path_.c_str());
    ESP_LOGCONFIG(TAG, "  Deletion Enabled: %s", TRUEFALSE(this->deletion_enabled_));
    ESP_LOGCONFIG(TAG, "  Download Enabled: %s", TRUEFALSE(this->download_enabled_));
    ESP_LOGCONFIG(TAG, "  Upload Enabled: %s", TRUEFALSE(this->upload_enabled_));
}

#ifdef USE_ESP_IDF
// Gestionnaires statiques pour ESP-IDF
esp_err_t SDFileServer::handle_get_static(httpd_req_t *req) {
    SDFileServer *server = static_cast<SDFileServer *>(req->user_ctx);
    return server->handle_get_idf(req);
}

esp_err_t SDFileServer::handle_delete_static(httpd_req_t *req) {
    SDFileServer *server = static_cast<SDFileServer *>(req->user_ctx);
    return server->handle_delete_idf(req);
}

esp_err_t SDFileServer::handle_post_static(httpd_req_t *req) {
    SDFileServer *server = static_cast<SDFileServer *>(req->user_ctx);
    return server->handle_post_idf(req);
}

// Implémentations ESP-IDF
esp_err_t SDFileServer::handle_get_idf(httpd_req_t *req) {
    std::string url(req->uri);
    std::string extracted = this->extract_path_from_url(url);
    std::string path = this->build_absolute_path(extracted);
    if (!this->sd_mmc_card_->is_directory(path)) {
        return this->handle_download_idf(req, path);
    }
    return this->handle_index_idf(req, path);
}

esp_err_t SDFileServer::handle_delete_idf(httpd_req_t *req) {
    if (!this->deletion_enabled_) {
        const char *response = "{ \"error\": \"file deletion is disabled\" }";
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_status(req, "401 Unauthorized");
        httpd_resp_send(req, response, strlen(response));
        return ESP_OK;
    }
    std::string url(req->uri);
    std::string extracted = this->extract_path_from_url(url);
    std::string path = this->build_absolute_path(extracted);
    if (this->sd_mmc_card_->is_directory(path)) {
        const char *response = "{ \"error\": \"cannot delete a directory\" }";
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_status(req, "401 Unauthorized");
        httpd_resp_send(req, response, strlen(response));
        return ESP_OK;
    }
    if (this->sd_mmc_card_->delete_file(path)) {
        httpd_resp_set_status(req, "204 No Content");
        httpd_resp_send(req, nullptr, 0);
        return ESP_OK;
    }
    const char *response = "{ \"error\": \"failed to delete file\" }";
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_status(req, "401 Unauthorized");
    httpd_resp_send(req, response, strlen(response));
    return ESP_OK;
}

esp_err_t SDFileServer::handle_post_idf(httpd_req_t *req) {
    if (!this->upload_enabled_) {
        const char *response = "{ \"error\": \"file upload is disabled\" }";
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_status(req, "401 Unauthorized");
        httpd_resp_send(req, response, strlen(response));
        return ESP_OK;
    }
    // Analyse du multipart/form-data
    char buf[1024];
    int ret, remaining = req->content_len;
    std::string url(req->uri);
    std::string extracted = this->extract_path_from_url(url);
    std::string path = this->build_absolute_path(extracted);
    std::string upload_file_path = path + "/uploaded_file";
    FILE *file = fopen(upload_file_path.c_str(), "wb");
    if (!file) {
        const char *response = "{ \"error\": \"failed to create file\" }";
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_send(req, response, strlen(response));
        return ESP_OK;
    }
    while (remaining > 0) {
        if ((ret = httpd_req_recv(req, buf, MIN(remaining, sizeof(buf)))) <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                continue;
            }
            fclose(file);
            const char *response = "{ \"error\": \"receive failed\" }";
            httpd_resp_set_type(req, "application/json");
            httpd_resp_set_status(req, "500 Internal Server Error");
            httpd_resp_send(req, response, strlen(response));
            return ESP_FAIL;
        }
        fwrite(buf, 1, ret, file);
        remaining -= ret;
    }
    fclose(file);
    const char *response = "{ \"success\": true }";
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_status(req, "201 Created");
    httpd_resp_send(req, response, strlen(response));
    return ESP_OK;
}

esp_err_t SDFileServer::handle_download_idf(httpd_req_t *req, const std::string &path) const {
    if (!this->download_enabled_) {
        const char *response = "{ \"error\": \"file download is disabled\" }";
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_status(req, "401 Unauthorized");
        httpd_resp_send(req, response, strlen(response));
        return ESP_OK;
    }
    auto file = this->sd_mmc_card_->read_file(path);
    if (file.size() == 0) {
        const char *response = "{ \"error\": \"failed to read file\" }";
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_status(req, "401 Unauthorized");
        httpd_resp_send(req, response, strlen(response));
        return ESP_OK;
    }
    httpd_resp_set_type(req, Path::mime_type(path).c_str());
    httpd_resp_send(req, (const char *)file.data(), file.size());
    return ESP_OK;
}

esp_err_t SDFileServer::handle_index_idf(httpd_req_t *req, const std::string &path) const {
    std::string html_start = R"(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<title>SD Card Files</title>
</head>
<body>
<div class="container">
)";
    std::string html_body = "<div class=\"header-actions\"><h1>SD Card Files</h1>";
    html_body += "<button onclick=\"window.location.href='/'\">Go to web server</button></div>";
    html_body += "<div class=\"breadcrumb\"><a href=\"/\">Home</a>";
    std::string current_path = "/";
    std::string relative_path = Path::join(this->url_prefix_, Path::remove_root_path(path, this->root_path_));
    std::vector<std::string> parts = Path::split_path(relative_path);
    for (std::string const &part : parts) {
        if (!part.empty()) {
            current_path = Path::join(current_path, part);
            html_body += "<a href=\"";
            html_body += current_path;
            html_body += "\">";
            html_body += part;
            html_body += "</a>";
        }
    }
    html_body += "</div>";
    if (this->upload_enabled_) {
        html_body += "<div class=\"upload-form\"><form method=\"POST\" enctype=\"multipart/form-data\">";
        html_body += "<input type=\"file\" name=\"file\"><input type=\"submit\" value=\"upload\"></form></div>";
    }
    html_body += "<table><thead><tr><th>Name</th><th>Type</th><th>Size</th><th>Actions</th></tr></thead><tbody>";
    auto entries = this->sd_mmc_card_->list_directory_file_info(path, 0);
    for (auto const &entry : entries) {
        std::string uri = "/" + Path::join(this->url_prefix_, Path::remove_root_path(entry.path, this->root_path_));
        std::string file_name = Path::file_name(entry.path);
        html_body += "<tr><td>";
        if (entry.is_directory) {
            html_body += "<a href=\"" + uri + "\">" + file_name + "</a>";
        } else {
            html_body += file_name;
        }
        html_body += "</td><td>";
        if (entry.is_directory) {
            html_body += "Folder";
        } else {
            html_body += "<span class=\"file-type\">" + Path::file_type(file_name) + "</span>";
        }
        html_body += "</td><td>";
        if (!entry.is_directory) {
            html_body += sd_mmc_card::format_size(entry.size);
        }
        html_body += "</td><td><div class=\"file-actions\">";
        if (!entry.is_directory) {
            if (this->download_enabled_) {
                html_body += "<button onClick=\"download_file('" + uri + "','" + file_name + "')\">Download</button>";
            }
            if (this->deletion_enabled_) {
                html_body += "<button onClick=\"delete_file('" + uri + "')\">Delete</button>";
            }
        }
        html_body += "<div></td></tr>";
    }
    html_body += "</tbody></table>";
    html_body += "<script>";
    html_body += "function delete_file(path) {fetch(path, {method: \"DELETE\"});}";
    html_body += "function download_file(path, filename) {";
    html_body += "fetch(path).then(response => response.blob())";
    html_body += ".then(blob => {";
    html_body += "const link = document.createElement('a');";
    html_body += "link.href = URL.createObjectURL(blob);";
    html_body += "link.download = filename;";
    html_body += "link.click();";
    html_body += "}).catch(console.error);";
    html_body += "} ";
    html_body += "</script>";
    html_body += "</body></html>";
    std::string html = html_start + html_body;
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, html.c_str(), html.length());
    return ESP_OK;
}
#else
// AsyncWebServer original code
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
    if (index == 0) {
        std::string extracted = this->extract_path_from_url(std::string(request->url().c_str()));
        std::string path = this->build_absolute_path(extracted);
        std::string dir = path.substr(0, path.find_last_of('/'));
        if (!this->sd_mmc_card_->is_directory(dir.c_str())) {
            ESP_LOGI(TAG, "Creating directory: %s", dir.c_str());
            if (!this->sd_mmc_card_->create_directory(dir.c_str())) {
                request->send(500, "text/plain", "Failed to create directory");
                return;
            }
        }
        upload_path = Path::join(path, filename.c_str());
        total_size = 0;
        use_chunked_mode = true;
        ESP_LOGI(TAG, "Starting upload: %s", upload_path.c_str());
    }
    size_t available_space = sizeof(current_chunk) - chunk_size;
    size_t bytes_to_copy = std::min(len, available_space);
    memcpy(current_chunk + chunk_size, data, bytes_to_copy);
    chunk_size += bytes_to_copy;
    if (chunk_size == sizeof(current_chunk) || final) {
        bool success = this->write_file_chunked(upload_path, [&](uint8_t *buffer, size_t max_len) -> size_t {
            size_t bytes_to_write = std::min(chunk_size, max_len);
            memcpy(buffer, current_chunk, bytes_to_write);
            memmove(current_chunk, current_chunk + bytes_to_write, chunk_size - bytes_to_write);
            chunk_size -= bytes_to_write;
            return bytes_to_write;
        }, 4096);
        if (!success) {
            request->send(500, "text/plain", "Write error in chunked mode");
            return;
        }
    }
    if (final) {
        ESP_LOGI(TAG, "Upload complete: %s (%d bytes)", upload_path.c_str(), total_size);
        auto response = request->beginResponse(201, "text/html", "upload success");
        response->addHeader("Connection", "close");
        request->send(response);
    }
}

void SDFileServer::handle_get(AsyncWebServerRequest *request) const {
    std::string extracted = this->extract_path_from_url(std::string(request->url().c_str()));
    std::string path = this->build_absolute_path(extracted);
    if (!this->sd_mmc_card_->is_directory(path)) {
        handle_download(request, path);
        return;
    }
    handle_index(request, path);
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
    if (info.is_directory) {
        response->print("Folder");
    } else {
        response->print("<span class=\"file-type\">");
        response->print(Path::file_type(file_name).c_str());
        response->print("</span>");
    }
    response->print("</td><td>");
    if (!info.is_directory) {
        response->print(sd_mmc_card::format_size(info.size).c_str());
    }
    response->print("</td><td><div class=\"file-actions\">");
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
    response->print("<div></td></tr>");
}

void SDFileServer::handle_index(AsyncWebServerRequest *request, const std::string &path) const {
    AsyncResponseStream *response = request->beginResponseStream("text/html");
    response->print(F(R"(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset=UTF-8>
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
box-shadow: 0 4px 12px rgba(0, 0, 0, 0.1);
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
.breadcrumb a:not(:last-child)::after {
display: inline-block;
margin: 0 .25rem;
content: ">";
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
<button onclick="window.location.href='/'">Go to web server</button>
</div>
<div class="breadcrumb">
<a href="/">Home</a>)"));
    std::string current_path = "/";
    std::string relative_path = Path::join(this->url_prefix_, Path::remove_root_path(path, this->root_path_));
    std::vector<std::string> parts = Path::split_path(relative_path);
    for (std::string const &part : parts) {
        if (!part.empty()) {
            current_path = Path::join(current_path, part);
            response->print("<a href=\"");
            response->print(current_path.c_str());
            response->print("\">");
            response->print(part.c_str());
            response->print("</a>");
        }
    }
    response->print(F("</div>"));
    if (this->upload_enabled_)
        response->print(F("<div class=\"upload-form\"><form method=\"POST\" enctype=\"multipart/form-data\">"
                          "<input type=\"file\" name=\"file\"><input type=\"submit\" value=\"upload\"></form></div>"));
    response->print(F("<table><thead><tr>"
                      "<th>Name</th>"
                      "<th>Type</th>"
                      "<th>Size</th>"
                      "<th>Actions</th>"
                      "</tr></thead><tbody>"));
    auto entries = this->sd_mmc_card_->list_directory_file_info(path, 0);
    for (auto const &entry)
        write_row(response, entry);
    response->print(F("</tbody></table>"
                      "<script>"
                      "function delete_file(path) {fetch(path, {method: \"DELETE\"});}"
                      "function download_file(path, filename) {"
                      "fetch(path).then(response => response.blob())"
                      ".then(blob => {"
                      "const link = document.createElement('a');"
                      "link.href = URL.createObjectURL(blob);"
                      "link.download = filename;"
                      "link.click();"
                      "}).catch(console.error);"
                      "} "
                      "</script>"
                      "</body></html>"));
    request->send(response);
}

void SDFileServer::handle_download(AsyncWebServerRequest *request, const std::string &path) const {
    if (!this->download_enabled_) {
        request->send(401, "application/json", "{ \"error\": \"file download is disabled\" }");
        return;
    }
    auto file = this->sd_mmc_card_->read_file(path);
    if (file.size() == 0) {
        request->send(401, "application/json", "{ \"error\": \"failed to read file\" }");
        return;
    }
#ifdef USE_ESP_IDF
    auto *response = request->beginResponse_P(200, Path::mime_type(path).c_str(), file.data(), file.size());
#else
    auto *response = request->beginResponseStream(Path::mime_type(path).c_str(), file.size());
    response->write(file.data(), file.size());
#endif
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

std::string SDFileServer::extract_path_from_url(const std::string &url) const {
    std::string prefix = this->build_prefix();
    return url.substr(prefix.size(), url.size() - prefix.size());
}

std::string SDFileServer::build_absolute_path(const std::string &relative_path) const {
    if (relative_path.size() == 0)
        return this->root_path_;
    std::string absolute = Path::join(this->root_path_, relative_path);
    return absolute;
}

bool SDFileServer::write_file_chunked(const std::string &path, std::function<size_t(uint8_t *, size_t)> writer,
                                      size_t max_len) {
    FILE *file = fopen(path.c_str(), "ab");
    if (!file) return false;

    uint8_t buffer[max_len];
    size_t bytes_written = 0;
    while (true) {
        size_t bytes_to_write = writer(buffer, max_len);
        if (bytes_to_write == 0) break;
        fwrite(buffer, 1, bytes_to_write, file);
        bytes_written += bytes_to_write;
    }

    fclose(file);
    return true;
}

#endif

std::string Path::file_name(const std::string &path) {
    size_t pos = path.rfind(Path::separator);
    if (pos != std::string::npos) {
        return path.substr(pos + 1);
    }
    return "";
}

bool Path::is_absolute(const std::string &path) { return path.size() && path[0] == separator; }

bool Path::trailing_slash(const std::string &path) {
    return path.size() && path[path.length() - 1] == separator;
}

std::string Path::join(const std::string &first, const std::string &second) {
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

std::string Path::remove_root_path(const std::string &path, const std::string &root) {
    if (!str_startswith(path, root))
        return path;
    if (path.size() == root.size() || path.size() < 2)
        return "/";
    return path.erase(0, root.size());
}

std::vector<std::string> Path::split_path(const std::string &path) {
    std::vector<std::string> parts;
    size_t pos = 0;
    while ((pos = path.find('/')) != std::string::npos) {
        std::string part = path.substr(0, pos);
        if (!part.empty()) {
            parts.push_back(part);
        }
        path.erase(0, pos + 1);
    }
    parts.push_back(path);
    return parts;
}

std::string Path::extension(const std::string &file) {
    size_t pos = file.find_last_of('.');
    if (pos == std::string::npos)
        return "";
    return file.substr(pos + 1);
}

std::string Path::file_type(const std::string &file) {
    static const std::map<std::string, std::string> file_types = {
        {"mp3", "Audio (MP3)"},   {"wav", "Audio (WAV)"}, {"png", "Image (PNG)"},   {"jpg", "Image (JPG)"},
        {"jpeg", "Image (JPEG)"}, {"bmp", "Image (BMP)"}, {"txt", "Text (TXT)"},    {"log", "Text (LOG)"},
        {"csv", "Text (CSV)"},    {"html", "Web (HTML)"}, {"css", "Web (CSS)"},     {"js", "Web (JS)"},
        {"json", "Data (JSON)"},  {"xml", "Data (XML)"},  {"zip", "Archive (ZIP)"}, {"gz", "Archive (GZ)"},
        {"tar", "Archive (TAR)"}, {"mp4", "Video (MP4)"}, {"avi", "Video (AVI)"},   {"webm", "Video (WEBM)"}};
    std::string ext = Path::extension(file);
    if (ext.empty())
        return "File";
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });
    auto it = file_types.find(ext);
    if (it != file_types.end())
        return it->second;
    return "File (" + ext + ")";
}

std::string Path::mime_type(const std::string &file) {
    static const std::map<std::string, std::string> file_types = {
        {"mp3", "audio/mpeg"},        {"wav", "audio/vnd.wav"},   {"png", "image/png"},       {"jpg", "image/jpeg"},
        {"jpeg", "image/jpeg"},       {"bmp", "image/bmp"},       {"txt", "text/plain"},      {"log", "text/plain"},
        {"csv", "text/csv"},          {"html", "text/html"},      {"css", "text/css"},        {"js", "text/javascript"},
        {"json", "application/json"}, {"xml", "application/xml"}, {"zip", "application/zip"}, {"gz", "application/gzip"},
        {"tar", "application/x-tar"}, {"mp4", "video/mp4"},       {"avi", "video/x-msvideo"}, {"webm", "video/webm"}};
    std::string ext = Path::extension(file);
    ESP_LOGD(TAG, "ext : %s", ext.c_str());
    if (!ext.empty()) {
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });
        auto it = file_types.find(ext);
        if (it != file_types.end())
            return it->second;
    }
    return "application/octet-stream";
}

}  // namespace sd_file_server
}  // namespace esphome












































































