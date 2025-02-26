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
  explicit SDCardUpdateTrigger(SDCard *sd_card) : sd_card_(sd_card) {
    // Créez un intervalle à la main si interval() n'est pas disponible
    // Exemple avec un délai de 1000 ms, ou utilisez votre propre logique
    this->interval_ = millis() + 1000;  // Stocke l'instant de mise à jour suivante
  }

  void update() override {
    // Assurez-vous que vous avez correctement initialisé `sd_card_` avant d'accéder à ses méthodes
    if (millis() >= this->interval_) {
      if (this->sd_card_) {
        this->sd_card_->update_sensors();  // Actualise les capteurs
      }
      this->interval_ = millis() + 1000;  // Réinitialise l'intervalle
    }
  }

 protected:
  SDCard *sd_card_;
  unsigned long interval_;  // Pour gérer l'intervalle manuellement
};

}  // namespace sd_card
}  // namespace esphome





