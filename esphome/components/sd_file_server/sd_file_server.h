#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sd_mmc_card/sd_mmc_card.h"
#include <esp_http_server.h>

namespace esphome {
namespace sd_file_server {

class SDFileServer : public Component {
 public:
  SDFileServer();

  void setup() override;
  void loop() override;
  void dump_config() override;

  void set_port(uint16_t port) { port_ = port; }
  void set_url_prefix(const std::string &prefix) { url_prefix_ = prefix; }
  void set_root_path(const std::string &path) { root_path_ = path; }
  void set_sd_mmc_card(sd_mmc_card::SdMmc *card) { sd_mmc_card_ = card; }
  void set_deletion_enabled(bool enabled) { deletion_enabled_ = enabled; }
  void set_download_enabled(bool enabled) { download_enabled_ = enabled; }
  void set_upload_enabled(bool enabled) { upload_enabled_ = enabled; }

 protected:
  httpd_handle_t server_{nullptr};
  sd_mmc_card::SdMmc *sd_mmc_card_{nullptr};
  uint16_t port_{80};
  std::string url_prefix_{"files"};
  std::string root_path_{"/sdcard"};
  bool deletion_enabled_{false};
  bool download_enabled_{true};
  bool upload_enabled_{false};

  void start_server();
  static esp_err_t index_handler(httpd_req_t *req);
  static esp_err_t file_handler(httpd_req_t *req);
  static esp_err_t delete_handler(httpd_req_t *req);
  static esp_err_t upload_handler(httpd_req_t *req);

  std::string build_prefix() const;
  std::string extract_path_from_url(const std::string &url) const;
  std::string build_absolute_path(const std::string &file_path) const;
};

}  // namespace sd_file_server
}  // namespace esphome





















