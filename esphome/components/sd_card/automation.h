#pragma once
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "sd_card.h"

namespace esphome {
namespace sd_card {

// Utilisez les types corrects pour l'action
template<typename... Ts>
class SDCardUpdateAction : public Action<Ts...> {
 public:
  explicit SDCardUpdateAction(SDCard *sd_card) : sd_card_(sd_card) {}
  
  void play(Ts... x) override {
    if (this->sd_card_) {
      this->sd_card_->update_sensors();
    }
  }
  
 protected:
  SDCard *sd_card_;
};

// Pour le trigger, héritez de PollingComponent
class SDCardUpdateTrigger : public PollingComponent, public Trigger<> {
 public:
  explicit SDCardUpdateTrigger(SDCard *sd_card) : sd_card_(sd_card) {}
  
  void update() override {
    if (this->sd_card_) {
      this->sd_card_->update_sensors();
      this->trigger();
    }
  }
  
  float get_setup_priority() const override { return setup_priority::DATA; }
  
 protected:
  SDCard *sd_card_;
};

}  // namespace sd_card
}  // namespace esphome





