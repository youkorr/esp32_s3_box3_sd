#pragma once
#include "esphome/core/automation.h"
#include "sd_card.h"

namespace esphome {
namespace sd_card {

template<typename... Ts>
class SDCardUpdateAction : public Action<Ts...> {
 public:
  explicit SDCardUpdateAction(SDCard *sd_card) : sd_card_(sd_card) {}
  
  void play(Ts... args) override { 
    if (this->sd_card_) {
      this->sd_card_->update_sensors(); 
    }
  }
  
 protected:
  SDCard *sd_card_;
};

template<typename... Ts> 
class SDCardUpdateTrigger : public Trigger<Ts...> {
 public:
  explicit SDCardUpdateTrigger(SDCard *sd_card) {
    if (sd_card != nullptr) {
      sd_card->add_on_update_callback([this]() { this->trigger(); });
    }
  }
};

}  // namespace sd_card
}  // namespace esphome





