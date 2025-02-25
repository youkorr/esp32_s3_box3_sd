import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.const import CONF_ID, CONF_PATH
from esphome.core import CORE

# Déclaration des composants
sd_mmc_card_ns = cg.esphome_ns.namespace("sd_mmc_card")
SdMmc = sd_mmc_card_ns.class_("SdMmc", cg.Component)

# Déclaration des actions
SdMmcPlayAudioAction = sd_mmc_card_ns.class_("SdMmcPlayAudioAction", automation.Action)
SdMmcDisplayPngAction = sd_mmc_card_ns.class_("SdMmcDisplayPngAction", automation.Action)

# Schéma de configuration pour l'action de lecture audio
SD_MMC_PLAY_AUDIO_ACTION_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.use_id(SdMmc),
    cv.Required(CONF_PATH): cv.templatable(cv.string_strict),
})

# Schéma de configuration pour l'action d'affichage d'image
SD_MMC_DISPLAY_PNG_ACTION_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.use_id(SdMmc),
    cv.Required(CONF_PATH): cv.templatable(cv.string_strict),
})

# Fonction pour enregistrer l'action de lecture audio
@automation.register_action("sd_mmc_card.play_audio", SdMmcPlayAudioAction, SD_MMC_PLAY_AUDIO_ACTION_SCHEMA)
async def sd_mmc_play_audio_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    path_ = await cg.templatable(config[CONF_PATH], args, cg.std_string)
    cg.add(var.set_path(path_))
    return var

# Fonction pour enregistrer l'action d'affichage d'image
@automation.register_action("sd_mmc_card.display_png", SdMmcDisplayPngAction, SD_MMC_DISPLAY_PNG_ACTION_SCHEMA)
async def sd_mmc_display_png_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    path_ = await cg.templatable(config[CONF_PATH], args, cg.std_string)
    cg.add(var.set_path(path_))
    return var

# Fonction pour monter la carte SD et lire un fichier audio ou afficher une image au démarrage
async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    # Montage de la carte SD
    cg.add(var.setup())

    # Lecture d'un fichier audio ou affichage d'une image au démarrage
    if CORE.is_esp32:
        cg.add(var.play_audio("/test.flac"))  # Lire un fichier audio
        cg.add(var.display_png("/test.png"))  # Afficher une image



