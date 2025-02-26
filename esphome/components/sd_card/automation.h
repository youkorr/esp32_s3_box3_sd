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
    // Utilisation d'un intervalle pour appeler la mise à jour
    this->interval(1000);  // Vérifie toutes les 1000 ms
  }

  void update() override {
    if (this->sd_card_) {
      this->sd_card_->update_sensors();  // Actualise les capteurs
    }
  }

 protected:
  SDCard *sd_card_;
};

}  // namespace sd_card
}  // namespace esphome



