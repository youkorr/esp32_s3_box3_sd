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
      this->sd_card_->update_sensors();  // Appelle une méthode d'update des capteurs
    }
  }

 protected:
  SDCard *sd_card_;  // Utilise la classe SDCard correctement
};

class SDCardUpdateTrigger : public Trigger<> {
 public:
  explicit SDCardUpdateTrigger(SDCard *sd_card) {
    if (sd_card) {
      // Assurez-vous que la méthode add_on_update_callback existe
      // sinon vous devrez gérer la mise à jour autrement
      // sd_card->add_on_update_callback([this]() { this->trigger(); });
    }
  }
};

}  // namespace sd_card
}  // namespace esphome


