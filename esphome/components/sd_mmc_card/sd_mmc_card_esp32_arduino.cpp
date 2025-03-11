#include "sd_mmc_card.h"

#ifdef USE_ESP32_FRAMEWORK_ARDUINO

#include "math.h"
#include "esphome/core/log.h"

#include "SD_MMC.h"
#include "FS.h"

#define SDCARD_PWR_CTRL GPIO_NUM_43

namespace esphome {
namespace sd_mmc_card {

static const char *TAG = "sd_mmc_card_esp32_arduino";

void SdMmc::setup() {
  // Enable SDCard power
  if (SDCARD_PWR_CTRL >= 0) {
    gpio_config_t gpio_cfg = {
      .pin_bit_mask = 1ULL << SDCARD_PWR_CTRL,
      .mode = GPIO_MODE_OUTPUT,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&gpio_cfg);
    gpio_set_level(static_cast<gpio_num_t>(SDCARD_PWR_CTRL), 0);
  }

  bool beginResult = this->mode_1bit_ ? SD_MMC.begin("/sdcard", this->mode_1bit_) : SD_MMC.begin();
  if (!beginResult) {
    this->init_error_ = ErrorCode::ERR_MOUNT;
    this->mark_failed();
    return;
  }

  uint8_t cardType = SD_MMC.cardType();

#ifdef USE_TEXT_SENSOR
  if (this->sd_card_type_text_sensor_ != nullptr)
    this->sd_card_type_text_sensor_->publish_state(sd_card_type_to_string(cardType));
#endif

  if (cardType == CARD_NONE) {
    this->init_error_ = ErrorCode::ERR_NO_CARD;
    this->mark_failed();
    return;
  }

  update_sensors();
}

bool SdMmc::read_file_chunked(const char *path, std::function<void(const uint8_t *buffer, size_t len)> callback, size_t chunk_size) {
  File file = SD_MMC.open(path);
  if (!file) {
    ESP_LOGE(TAG, "Failed to open file for reading");
    return false;
  }

  size_t fileSize = file.size();
  uint8_t *buffer = new uint8_t[chunk_size];

  while (file.available()) {
    size_t bytesRead = file.read(buffer, chunk_size);
    if (bytesRead > 0) {
      callback(buffer, bytesRead);
    }
  }

  delete[] buffer;
  file.close();
  return true;
}

}  // namespace sd_mmc_card
}  // namespace esphome

#endif  // USE_ESP32_FRAMEWORK_ARDUINO
