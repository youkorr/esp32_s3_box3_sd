#pragma once

#include "esphome/core/automation.h"
#include "sd_card.h"

namespace esphome {
namespace sd_card {

class SDCardUpdateAction : public Action {
 public:
  explicit SDCardUpdateAction(SDCard *sd_card) : sd_card_(sd_card) {}
  
  void play() override { 
    if (this->sd_card_) {
      this->sd_card_->update_sensors(); 
    }
  }
  
 protected:
  SDCard *sd_card_;
};

class SDCardUpdateTrigger : public Trigger<> {
 public:
  explicit SDCardUpdateTrigger(SDCard *sd_card) {
    if (sd_card) {
      // On suppose ici que add_on_update_callback() existe et fonctionne
      sd_card->add_on_update_callback([this]() { this->trigger(); });
    }
  }
};

}  // namespace sd_card
}  // namespace esphome

