#pragma once

#include "esphome/core/component.h"
#include "esphome/components/web_server_base/web_server_base.h"
#include "esphome/components/sd_mmc_card/sd_mmc_card.h"

#ifdef USE_ESP_IDF
#include "esp_http_server.h"
#endif

namespace esphome {
namespace sd_file_server {

class SDFileServer : public Component {
public:
    explicit SDFileServer(web_server_base::WebServerBase *base);
#ifdef USE_ESP_IDF
    explicit SDFileServer(httpd_handle_t server);
#endif

    void setup() override;
    void dump_config() override;

#ifdef USE_ESP_IDF
    static esp_err_t handle_get_static(httpd_req_t *req);
    static esp_err_t handle_delete_static(httpd_req_t *req);
    static esp_err_t handle_post_static(httpd_req_t *req);

    esp_err_t handle_get_idf(httpd_req_t *req);
    esp_err_t handle_delete_idf(httpd_req_t *req);
    esp_err_t handle_post_idf(httpd_req_t *req);
    esp_err_t handle_download_idf(httpd_req_t *req, const std::string &path) const;
    esp_err_t handle_index_idf(httpd_req_t *req, const std::string &path) const;
#else
    bool canHandle(AsyncWebServerRequest *request) override;
    void handleRequest(AsyncWebServerRequest *request) override;
    void handleUpload(AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data,
                      size_t len, bool final) override;
    bool isRequestHandlerTrivial() override { return false; }
#endif

    void set_url_prefix(const std::string &);
    void set_root_path(const std::string &);
    void set_sd_mmc_card(sd_mmc_card::SDMMCCard *);
    void set_deletion_enabled(bool);
    void set_download_enabled(bool);
    void set_upload_enabled(bool);

protected:
#ifdef USE_ESP_IDF
    httpd_handle_t server_ = nullptr;
#else
    web_server_base::WebServerBase *base_ = nullptr;
#endif

    sd_mmc_card::SDMMCCard *sd_mmc_card_ = nullptr;

    std::string url_prefix_;
    std::string root_path_;
    bool deletion_enabled_ = false;
    bool download_enabled_ = false;
    bool upload_enabled_ = false;

    std::string build_prefix() const;
    std::string extract_path_from_url(const std::string &) const;
    std::string build_absolute_path(const std::string &) const;
    void write_row(AsyncResponseStream *response, const sd_mmc_card::FileInfo &info) const;
    void handle_index(AsyncWebServerRequest *, const std::string &) const;
    void handle_get(AsyncWebServerRequest *) const;
    void handle_delete(AsyncWebServerRequest *);
    void handle_download(AsyncWebServerRequest *, const std::string &) const;

    bool write_file_chunked(const std::string &path, const std::function<size_t(uint8_t *, size_t)> &data_provider,
                            size_t chunk_size);
};

struct Path {
    static constexpr char separator = '/';

    static std::string file_name(const std::string &);
    static bool is_absolute(const std::string &);
    static bool trailing_slash(const std::string &);
    static std::string join(const std::string &, const std::string &);
    static std::string remove_root_path(std::string, const std::string &);
    static std::vector<std::string> split_path(const std::string &);
    static std::string extension(const std::string &);
    static std::string file_type(const std::string &);
    static std::string mime_type(const std::string &);
};

}  // namespace sd_file_server
}  // namespace esphome





















