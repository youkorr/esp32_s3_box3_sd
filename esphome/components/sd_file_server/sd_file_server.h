#ifndef SD_FILE_SERVER_H
#define SD_FILE_SERVER_H

#include "esphome/core/component.h"
#include "esphome/components/web_server_base/web_server_base.h"
#include "esphome/components/sd_mmc_card/sd_mmc_card.h"
#include <string>
#include <vector>

namespace esphome {
namespace sd_file_server {

extern const char *const TAG;

class SDFileServer : public Component, public web_server_base::AsyncWebHandler {
 public:
  SDFileServer(web_server_base::WebServerBase *base);
  void setup() override;
  void dump_config() override;
  bool canHandle(web_server_base::AsyncWebServerRequest *request) override;
  void handleRequest(web_server_base::AsyncWebServerRequest *request) override;
  void handleUpload(web_server_base::AsyncWebServerRequest *request, const String &filename, size_t index,
                    uint8_t *data, size_t len, bool final) override;
  bool isRequestHandlerTrivial() override { return false; }

  void set_url_prefix(std::string const &prefix);
  void set_root_path(std::string const &path);
  void set_sd_mmc_card(sd_mmc_card::SdMmc *card);
  void set_deletion_enabled(bool allow);
  void set_download_enabled(bool allow);
  void set_upload_enabled(bool allow);

 protected:
  web_server_base::WebServerBase *base_;
  sd_mmc_card::SdMmc *sd_mmc_card_;

  std::string url_prefix_;
  std::string root_path_;
  bool deletion_enabled_{false};
  bool download_enabled_{true};
  bool upload_enabled_{false};

  std::string build_prefix() const;
  std::string extract_path_from_url(std::string const &url) const;
  std::string build_absolute_path(std::string relative_path) const;
  void write_row(web_server_base::AsyncResponseStream *response, sd_mmc_card::FileInfo const &info) const;
  void handle_index(web_server_base::AsyncWebServerRequest *request, std::string const &path) const;
  void handle_get(web_server_base::AsyncWebServerRequest *request) const;
  void handle_delete(web_server_base::AsyncWebServerRequest *request);
  void handle_download(web_server_base::AsyncWebServerRequest *request, std::string const &path) const;
};

struct Path {
  static constexpr char separator = '/';

  /* Return the name of the file */
  static std::string file_name(std::string const &path);

  /* Is the path an absolute path? */
  static bool is_absolute(std::string const &path);

  /* Does the path have a trailing slash? */
  static bool trailing_slash(std::string const &path);

  /* Join two path */
  static std::string join(std::string const &first, std::string const &second);

  static std::string remove_root_path(std::string path, std::string const &root);
};

}  // namespace sd_file_server
}  // namespace esphome

#endif



