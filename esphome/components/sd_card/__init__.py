import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation, pins
from esphome.const import (
    CONF_ID,
    CONF_DATA,
    CONF_PATH,
    CONF_CLK_PIN,
    CONF_INPUT,
    CONF_OUTPUT,
)
from esphome.core import CORE

# Définir les constantes pour les pins
CONF_SD_MMC_CARD_ID = "sd_mmc_card_id"
CONF_CMD_PIN = "cmd_pin"
CONF_DATA0_PIN = "data0_pin"
CONF_DATA1_PIN = "data1_pin"
CONF_DATA2_PIN = "data2_pin"
CONF_DATA3_PIN = "data3_pin"
CONF_MODE_1BIT = "mode_1bit"

# Création d'un espace de noms pour "sd_card"
sd_card_component_ns = cg.esphome_ns.namespace("sd_card")
SdCard = sd_card_component_ns.class_("SdCard", cg.Component)

# Actions
SdCardWriteFileAction = sd_card_component_ns.class_("SdCardWriteFileAction", automation.Action)
SdCardAppendFileAction = sd_card_component_ns.class_("SdCardAppendFileAction", automation.Action)
SdCardCreateDirectoryAction = sd_card_component_ns.class_("SdCardCreateDirectoryAction", automation.Action)
SdCardRemoveDirectoryAction = sd_card_component_ns.class_("SdCardRemoveDirectoryAction", automation.Action)
SdCardDeleteFileAction = sd_card_component_ns.class_("SdCardDeleteFileAction", automation.Action)

# Fonction de validation pour les données brutes
def validate_raw_data(value):
    if isinstance(value, str):
        return value.encode("utf-8")
    if isinstance(value, list):
        return cv.Schema([cv.hex_uint8_t])(value)
    raise cv.Invalid(
        "data must either be a string wrapped in quotes or a list of bytes"
    )

# Schéma de configuration pour "sd_card"
CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(SdCard),
        cv.Required(CONF_CLK_PIN): pins.internal_gpio_output_pin_number,
        cv.Required(CONF_CMD_PIN): pins.internal_gpio_output_pin_number,
        cv.Required(CONF_DATA0_PIN): pins.internal_gpio_pin_number({CONF_OUTPUT: True, CONF_INPUT: True}),
        cv.Optional(CONF_DATA1_PIN): pins.internal_gpio_pin_number({CONF_OUTPUT: True, CONF_INPUT: True}),
        cv.Optional(CONF_DATA2_PIN): pins.internal_gpio_pin_number({CONF_OUTPUT: True, CONF_INPUT: True}),
        cv.Optional(CONF_DATA3_PIN): pins.internal_gpio_pin_number({CONF_OUTPUT: True, CONF_INPUT: True}),
        cv.Optional(CONF_MODE_1BIT, default=False): cv.boolean,
    }
).extend(cv.COMPONENT_SCHEMA)

# Fonction asynchrone pour générer le code
async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_mode_1bit(config[CONF_MODE_1BIT]))

    cg.add(var.set_clk_pin(config[CONF_CLK_PIN]))
    cg.add(var.set_cmd_pin(config[CONF_CMD_PIN]))
    cg.add(var.set_data0_pin(config[CONF_DATA0_PIN]))

    if not config[CONF_MODE_1BIT]:
        cg.add(var.set_data1_pin(config[CONF_DATA1_PIN]))
        cg.add(var.set_data2_pin(config[CONF_DATA2_PIN]))
        cg.add(var.set_data3_pin(config[CONF_DATA3_PIN]))

    if CORE.using_arduino:
        if CORE.is_esp32:
            cg.add_library("FS", None)
            cg.add_library("SD_MMC", None)

# Schéma d'action pour les chemins de fichiers SD
SD_CARD_PATH_ACTION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(SdCard),
        cv.Required(CONF_PATH): cv.templatable(cv.string_strict),
    }
)

# Schéma d'action pour l'écriture dans un fichier SD
SD_CARD_WRITE_FILE_ACTION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(SdCard),
        cv.Required(CONF_PATH): cv.templatable(cv.string_strict),
        cv.Required(CONF_DATA): cv.templatable(validate_raw_data),
    }
).extend(SD_CARD_PATH_ACTION_SCHEMA)

# Définir l'action d'écriture de fichier SD
@automation.register_action(
    "sd_card.write_file", SdCardWriteFileAction, SD_CARD_WRITE_FILE_ACTION_SCHEMA
)
async def sd_card_write_file_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    path_ = await cg.templatable(config[CONF_PATH], args, cg.std_string)
    data_ = await cg.templatable(config[CONF_DATA], args, cg.std_vector.template(cg.uint8))
    cg.add(var.set_path(path_))
    cg.add(var.set_data(data_))
    return var

# Définir l'action d'ajout de fichier SD
@automation.register_action(
    "sd_card.append_file", SdCardAppendFileAction, SD_CARD_WRITE_FILE_ACTION_SCHEMA
)
async def sd_card_append_file_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    path_ = await cg.templatable(config[CONF_PATH], args, cg.std_string)
    data_ = await cg.templatable(config[CONF_DATA], args, cg.std_vector.template(cg.uint8))
    cg.add(var.set_path(path_))
    cg.add(var.set_data(data_))
    return var

# Définir l'action pour créer un répertoire SD
@automation.register_action(
    "sd_card.create_directory", SdCardCreateDirectoryAction, SD_CARD_PATH_ACTION_SCHEMA
)
async def sd_card_create_directory_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    path_ = await cg.templatable(config[CONF_PATH], args, cg.std_string)
    cg.add(var.set_path(path_))
    return var

# Définir l'action pour supprimer un répertoire SD
@automation.register_action(
    "sd_card.remove_directory", SdCardRemoveDirectoryAction, SD_CARD_PATH_ACTION_SCHEMA
)
async def sd_card_remove_directory_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    path_ = await cg.templatable(config[CONF_PATH], args, cg.std_string)
    cg.add(var.set_path(path_))
    return var

# Définir l'action pour supprimer un fichier SD
@automation.register_action(
    "sd_card.delete_file", SdCardDeleteFileAction, SD_CARD_PATH_ACTION_SCHEMA
)
async def sd_card_delete_file_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    path_ = await cg.templatable(config[CONF_PATH], args, cg.std_string)
    cg.add(var.set_path(path_))
    return var














