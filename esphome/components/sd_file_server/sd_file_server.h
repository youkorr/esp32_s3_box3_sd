#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/http.h"
#include "esphome/components/web_server_base/web_server_base.h"
#include "esphome/components/sd_mmc_card/sd_mmc_card.h" // Include SdMmc
#include

namespace esphome {
namespace sd_file_server {

struct Path {
  static constexpr char separator = '/';

  static std::string file_name(std::string const &path);
  static bool is_absolute(std::string const &path);
  static bool trailing_slash(std::string const &path);
  static std::string join(std::string const &first, std::string const &second);
  static std::string remove_root_path(std::string path, std::string const &root);
};

class SDFileServer : public Component, public web_server_base::AsyncWebHandler {
 public:
  SDFileServer(web_server_base::WebServerBase *base);
  SDFileServer(web_server_base::WebServerBase *base, sd_mmc_card::SdMmc *sd_mmc); // Add constructor

  void setup() override;
  void dump_config() override;
  void set_url_prefix(const std::string &url_prefix);

  void set_sd_mmc(sd_mmc_card::SdMmc *sd_mmc); // Add set_sd_mmc

  // Implementation of AsyncWebHandler
  bool canHandle(AsyncWebServerRequest *request) override;
  void handle_web_request(AsyncWebServerRequest *request) override;

 protected:
  virtual void handleUpload(AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len, bool final);
  virtual void handleDownload(AsyncWebServerRequest *request);

  web_server_base::WebServerBase *base_{nullptr};
  sd_mmc_card::SdMmc *sd_mmc_{nullptr}; // Add SdMmc member

  std::string url_prefix_{};
};

}  // namespace sd_file_server
}  // namespace esphome






















