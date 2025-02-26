#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "sd_card.h"

namespace esphome {
namespace sd_box_card {

class SDCardListFilesAction : public Action<> {
 public:
  explicit SDCardListFilesAction(SDBoxCard *card) : card_(card) {}

  void play(Ts... x) override {
    // Implement file listing logic here
  }

 protected:
  SDBoxCard *card_;
};

}  // namespace sd_box_card
}  // namespace esphome

