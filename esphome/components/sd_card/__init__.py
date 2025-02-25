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
SdMmcLoadImageAction = sd_mmc_card_ns.class_("SdMmcLoadImageAction", automation.Action)

# Schéma de configuration pour l'action de lecture audio
SD_MMC_PLAY_AUDIO_ACTION_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.use_id(SdMmc),
    cv.Required(CONF_PATH): cv.templatable(cv.string_strict),
})

# Schéma de configuration pour l'action de chargement d'image
SD_MMC_LOAD_IMAGE_ACTION_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.use_id(SdMmc),
    cv.Required(CONF_PATH): cv.templatable(cv.string_strict),
})

# Définition de la constante CONF_AUTO_MOUNT
CONF_AUTO_MOUNT = "auto_mount"

# Schéma de configuration pour le composant SD Card
CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(SdMmc),
    cv.Optional(CONF_AUTO_MOUNT, default=True): cv.boolean,  # Montage automatique activé par défaut
    cv.Required("mode"): cv.enum({"SPI": "SPI", "MMC": "MMC"}),  # Mode de communication (SPI ou MMC)
    cv.Required("cs_pin"): cv.int_range(min=0, max=40),  # Broche Chip Select (CS)
    cv.Optional("mosi_pin"): cv.int_range(min=0, max=40),  # Broche MOSI (Data In)
    cv.Optional("miso_pin"): cv.int_range(min=0, max=40),  # Broche MISO (Data Out)
    cv.Required("clk_pin"): cv.int_range(min=0, max=40),  # Broche Clock (CLK)
}).extend(cv.COMPONENT_SCHEMA)

# Fonction pour enregistrer l'action de lecture audio
@automation.register_action("sd_mmc_card.play_audio", SdMmcPlayAudioAction, SD_MMC_PLAY_AUDIO_ACTION_SCHEMA)
async def sd_mmc_play_audio_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    path_ = await cg.templatable(config[CONF_PATH], args, cg.std_string)
    cg.add(var.set_path(path_))
    return var

# Fonction pour enregistrer l'action de chargement d'image
@automation.register_action("sd_mmc_card.load_image", SdMmcLoadImageAction, SD_MMC_LOAD_IMAGE_ACTION_SCHEMA)
async def sd_mmc_load_image_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    path_ = await cg.templatable(config[CONF_PATH], args, cg.std_string)
    cg.add(var.set_path(path_))
    return var

# Fonction pour monter la carte SD au démarrage
async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    # Configuration des broches
    cg.add(var.set_cs_pin(config["cs_pin"]))
    cg.add(var.set_clk_pin(config["clk_pin"]))

    if "mosi_pin" in config:
        cg.add(var.set_mosi_pin(config["mosi_pin"]))

    if "miso_pin" in config:
        cg.add(var.set_miso_pin(config["miso_pin"]))

    # Montage automatique de la carte SD
    if config[CONF_AUTO_MOUNT]:
        cg.add(var.setup())



