#pragma once
#include "esphome/core/component.h"
#include "esphome/components/web_server_base/web_server_base.h"
#include "../sd_mmc_card/sd_mmc_card.h"

namespace esphome {
namespace sd_file_server {

class SDFileServer : public Component, public AsyncWebHandler {
 public:
  SDFileServer(web_server_base::WebServerBase *base);
  void setup() override;
  void dump_config() override;
  bool canHandle(AsyncWebServerRequest *request) override;
  void handleRequest(AsyncWebServerRequest *request) override;
  bool isRequestHandlerTrivial() override { return false; }

  void set_url_prefix(std::string const &prefix);
  void set_root_path(std::string const &path);
  void set_sd_mmc_card(sd_mmc_card::SdMmc *card);
  void set_download_enabled(bool allow);

 protected:
  web_server_base::WebServerBase *base_;
  sd_mmc_card::SdMmc *sd_mmc_card_;

  std::string url_prefix_;
  std::string root_path_;
  bool download_enabled_;

  std::string build_prefix() const;
  std::string extract_path_from_url(std::string const &url) const;
  std::string build_absolute_path(std::string relative_path) const;
  void write_row(AsyncResponseStream *response, sd_mmc_card::FileInfo const &info) const;
  void handle_index(AsyncWebServerRequest *request, std::string const &path) const;
  void handle_get(AsyncWebServerRequest *request) const;
  void handle_download(AsyncWebServerRequest *request, std::string const &path) const;
};

struct Path {
  static constexpr char separator = '/';

  static std::string file_name(std::string const &path);
  static bool is_absolute(std::string const &path);
  static bool trailing_slash(std::string const &path);
  static std::string join(std::string const &first, std::string const &second);
  static std::string remove_root_path(std::string path, std::string const &root);
};

}  // namespace sd_file_server
}  // namespace esphome

