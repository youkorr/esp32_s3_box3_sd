#pragma once

#include "esphome/core/automation.h"
#include "sd_card.h"

namespace esphome {
namespace sd_card {

class SDCardUpdateAction : public Action<> {
 public:
  explicit SDCardUpdateAction(SDCard *sd_card) : sd_card_(sd_card) {}
  
  void play(Ts...) override { this->sd_card_->update_sensors(); }
  
 protected:
  SDCard *sd_card_;
};

template<typename... Ts> class SDCardUpdateTrigger : public Trigger<Ts...> {
 public:
  explicit SDCardUpdateTrigger(SDCard *sd_card) {
    sd_card->add_on_update_callback([this]() { this->trigger(); });
  }
};

}  // namespace sd_card
}  // namespace esphome
