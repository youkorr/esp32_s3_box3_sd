#pragma once

#include "esphome/core/automation.h"
#include "sd_card.h"

namespace esphome {
namespace sd_card {

// Action pour mettre à jour la carte SD
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

// Déclencheur pour mettre à jour la carte SD
class SDCardUpdateTrigger : public Trigger<> {
 public:
  explicit SDCardUpdateTrigger(SDCard *sd_card) : sd_card_(sd_card) {
    // Créez un intervalle à la main si interval() n'est pas disponible
    this->interval_ = millis() + 1000;  // Stocke l'instant de mise à jour suivante
  }

  void update() override {
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

// Actions personnalisées pour la carte SD (ajoutées depuis le code d'aide)

class SdMmcWriteFileAction : public Action {
 public:
  explicit SdMmcWriteFileAction(SDCard *sd_card) : sd_card_(sd_card) {}

  void play() override {
    if (this->sd_card_) {
      std::string path = "example.txt";  // Exemple de chemin, ajustez-le selon vos besoins
      std::vector<uint8_t> data = {0x01, 0x02, 0x03};  // Exemple de données à écrire
      this->sd_card_->write_file(path, data);  // Méthode hypothétique
    }
  }

 protected:
  SDCard *sd_card_;  // Utilisation de la classe SDCard
};

class SdMmcAppendFileAction : public Action {
 public:
  explicit SdMmcAppendFileAction(SDCard *sd_card) : sd_card_(sd_card) {}

  void play() override {
    if (this->sd_card_) {
      std::string path = "example.txt";  // Exemple de chemin
      std::vector<uint8_t> data = {0x04, 0x05, 0x06};  // Exemple de données à ajouter
      this->sd_card_->append_to_file(path, data);  // Méthode hypothétique
    }
  }

 protected:
  SDCard *sd_card_;
};

class SdMmcCreateDirectoryAction : public Action {
 public:
  explicit SdMmcCreateDirectoryAction(SDCard *sd_card) : sd_card_(sd_card) {}

  void play() override {
    if (this->sd_card_) {
      std::string path = "new_directory";  // Exemple de répertoire
      this->sd_card_->create_directory(path);  // Méthode hypothétique
    }
  }

 protected:
  SDCard *sd_card_;
};

class SdMmcRemoveDirectoryAction : public Action {
 public:
  explicit SdMmcRemoveDirectoryAction(SDCard *sd_card) : sd_card_(sd_card) {}

  void play() override {
    if (this->sd_card_) {
      std::string path = "old_directory";  // Exemple de répertoire à supprimer
      this->sd_card_->remove_directory(path);  // Méthode hypothétique
    }
  }

 protected:
  SDCard *sd_card_;
};

class SdMmcDeleteFileAction : public Action {
 public:
  explicit SdMmcDeleteFileAction(SDCard *sd_card) : sd_card_(sd_card) {}

  void play() override {
    if (this->sd_card_) {
      std::string path = "file_to_delete.txt";  // Exemple de fichier à supprimer
      this->sd_card_->delete_file(path);  // Méthode hypothétique
    }
  }

 protected:
  SDCard *sd_card_;
};

}  // namespace sd_card
}  // namespace esphome




