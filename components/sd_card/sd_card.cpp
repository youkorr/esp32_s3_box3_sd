#include "sd_card.h"
#include "esphome/core/log.h"

namespace esphome {
namespace esp32_s3_box3_sd {

static const char *TAG = "esp32_s3_box3_sd";

void ESP32S3Box3SDCard::setup() {
  ESP_LOGCONFIG(TAG, "Setting up SD Card...");

  // Initialize SD Card
  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);

  if (!SD.begin(SD_CS, SPI)) {
    ESP_LOGE(TAG, "Card Mount Failed");
    return;
  }

  uint8_t cardType = SD.cardType();
  if (cardType == CARD_NONE) {
    ESP_LOGE(TAG, "No SD card attached");
    return;
  }

  ESP_LOGI(TAG, "SD Card Type: %s", SD.cardTypeName(cardType));
  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  ESP_LOGI(TAG, "SD Card Size: %lluMB", cardSize);

  this->update_sensors_();
}

void ESP32S3Box3SDCard::loop() { this->update_sensors_(); }

void ESP32S3Box3SDCard::dump_config() {
  ESP_LOGCONFIG(TAG, "SD Card Component:");
  ESP_LOGCONFIG(TAG, "  CS Pin: %d", SD_CS);
  ESP_LOGCONFIG(TAG, "  MOSI Pin: %d", SD_MOSI);
  ESP_LOGCONFIG(TAG, "  MISO Pin: %d", SD_MISO);
  ESP_LOGCONFIG(TAG, "  SCK Pin: %d", SD_SCK);
}

void ESP32S3Box3SDCard::update_sensors_() {
  if (this->space_used_sensor_ != nullptr) {
    float used_space = (SD.usedBytes() / (float)SD.totalBytes()) * 100.0f;
    this->space_used_sensor_->publish_state(used_space);
  }

  if (this->total_space_sensor_ != nullptr) {
    float total_space = SD.totalBytes() / (1024.0f * 1024.0f);
    this->total_space_sensor_->publish_state(total_space);
  }
}

bool ESP32S3Box3SDCard::sdcard_is_mounted() {
  return SD.cardType() != CARD_NONE;
}

}  // namespace esp32_s3_box3_sd
}  // namespace esphome



