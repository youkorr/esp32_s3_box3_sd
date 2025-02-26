#pragma once

#include "esphome/core/automation.h"
#include "sd_card.h"

namespace esphome {
namespace sd_card {

class SDCardUpdateAction : public Action<> {
 public:
  explicit SDCardUpdateAction(SDCard *sd_card) : sd_card_(sd_card) {}

  void play(Ts...) override {
    if (this->sd_card_) {
      this->sd_card_->update_sensors();  // Appelle la méthode update_sensors() sur l'objet SDCard
    }
  }

 protected:
  SDCard *sd_card_;  // Utilise la classe SDCard correctement
};

template<typename... Ts> 
class SDCardUpdateTrigger : public Trigger<Ts...> {
 public:
  explicit SDCardUpdateTrigger(SDCard *sd_card) {
    // Utilise une mise à jour périodique au lieu de add_on_update_callback()
    this->interval(1000);  // Vérifie toutes les 1000 ms (1 seconde)
  }

  void update() override {
    if (this->sd_card_) {
      this->sd_card_->update_sensors();  // Actualise les capteurs toutes les 1000 ms
    }
  }

 protected:
  SDCard *sd_card_;  // Utilise correctement le pointeur vers SDCard
};

}  // namespace sd_card
}  // namespace esphome






