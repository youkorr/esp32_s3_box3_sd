#include "esphome.h"
#include "SD.h"

namespace esphome {
namespace esp32_s3_box3_sd {

class SDStatusSensor : public Component, public binary_sensor::BinarySensor {
 public:
  void setup() override {
    bool card_present = (SD.cardType() != CARD_NONE);
    publish_state(card_present);
  }

  void loop() override {
    bool card_present = (SD.cardType() != CARD_NONE);
    publish_state(card_present);
  }
};

}  // namespace esp32_s3_box3_sd
}  // namespace esphome
