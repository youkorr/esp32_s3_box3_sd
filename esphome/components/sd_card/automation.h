#pragma once
#include "esphome/core/automation.h"
#include "sd_card.h"
#include "esphome/core/component.h"

namespace esphome {
namespace sd_card {

// Action doit inclure le template correct
class SDCardUpdateAction : public Action<> {
 public:
  explicit SDCardUpdateAction(SDCard *sd_card) : sd_card_(sd_card) {}
  
  void play(UpdateType type) override {
    if (this->sd_card_) {
      this->sd_card_->update_sensors();
    }
  }
  
 protected:
  SDCard *sd_card_;
};

// Trigger doit inclure Component pour utiliser update()
class SDCardUpdateTrigger : public Trigger<>, public Component {
 public:
  explicit SDCardUpdateTrigger(SDCard *sd_card) : sd_card_(sd_card) {
    this->interval_ = 1000; // Intervalle de 1000ms
  }
  
  void setup() override {
    // Configuration initiale si nécessaire
  }
  
  void update() override {
    const uint32_t now = millis();
    if (now - this->last_update_ >= this->interval_) {
      if (this->sd_card_) {
        this->sd_card_->update_sensors();
        this->trigger();  // Déclenche le trigger
      }
      this->last_update_ = now;
    }
  }
  
  void set_interval(uint32_t interval) { this->interval_ = interval; }
  
 protected:
  SDCard *sd_card_;
  uint32_t interval_{1000};
  uint32_t last_update_{0};
};

}  // namespace sd_card
}  // namespace esphome





