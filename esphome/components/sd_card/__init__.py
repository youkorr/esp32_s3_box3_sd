import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_CLK_PIN,
    CONF_CMD_PIN,
    CONF_DATA0_PIN,
    CONF_DATA1_PIN,
    CONF_DATA2_PIN,
    CONF_DATA3_PIN,
    CONF_MODE_1BIT
)
from esphome.components import automation
from esphome.core import CORE
from esphome.pins import gpio_pin

# Nom de l'espace de noms pour l'élément sd_card
sd_card_component_ns = cg.esphome_ns.namespace("sd_card")
SDCard = sd_card_component_ns.class_("SDCard", cg.Component)

# Action pour les fichiers SD
SdCardWriteFileAction = sd_card_component_ns.class_("SdCardWriteFileAction", automation.Action)
SdCardAppendFileAction = sd_card_component_ns.class_("SdCardAppendFileAction", automation.Action)
SdCardCreateDirectoryAction = sd_card_component_ns.class_("SdCardCreateDirectoryAction", automation.Action)
SdCardRemoveDirectoryAction = sd_card_component_ns.class_("SdCardRemoveDirectoryAction", automation.Action)
SdCardDeleteFileAction = sd_card_component_ns.class_("SdCardDeleteFileAction", automation.Action)

def validate_raw_data(value):
    if isinstance(value, str):
        return value.encode("utf-8")
    if isinstance(value, list):
        return cv.Schema([cv.hex_uint8_t])(value)
    raise cv.Invalid(
        "data must either be a string wrapped in quotes or a list of bytes"
    )

# Schema de configuration
CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(SDCard),
        cv.Required(CONF_CLK_PIN): gpio_pin,
        cv.Required(CONF_CMD_PIN): gpio_pin,
        cv.Required(CONF_DATA0_PIN): gpio_pin,
        cv.Optional(CONF_DATA1_PIN): gpio_pin,
        cv.Optional(CONF_DATA2_PIN): gpio_pin,
        cv.Optional(CONF_DATA3_PIN): gpio_pin,
        cv.Optional(CONF_MODE_1BIT, default=False): cv.boolean,
    }
).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    # Ajouter les pins au code
    cg.add(var.set_mode_1bit(config[CONF_MODE_1BIT]))
    cg.add(var.set_clk_pin(config[CONF_CLK_PIN]))
    cg.add(var.set_cmd_pin(config[CONF_CMD_PIN]))
    cg.add(var.set_data0_pin(config[CONF_DATA0_PIN]))

    # Si le mode 1bit est activé, ajouter les pins de données supplémentaires
    if not config[CONF_MODE_1BIT]:
        cg.add(var.set_data1_pin(config[CONF_DATA1_PIN]))
        cg.add(var.set_data2_pin(config[CONF_DATA2_PIN]))
        cg.add(var.set_data3_pin(config[CONF_DATA3_PIN]))

    if CONF_CARD_TYPE in sd_config:
        ct_sens = await cg.get_variable(sd_config[CONF_CARD_TYPE])
        cg.add(var.set_card_type_sensor(ct_sens))

    if CONF_TOTAL_SPACE in sd_config:
        ts_sens = await cg.get_variable(sd_config[CONF_TOTAL_SPACE])
        cg.add(var.set_total_space_sensor(ts_sens))

    if CONF_USED_SPACE in sd_config:
        us_sens = await cg.get_variable(sd_config[CONF_USED_SPACE])
        cg.add(var.set_used_space_sensor(us_sens))

    if CONF_FREE_SPACE in sd_config:
        fs_sens = await cg.get_variable(sd_config[CONF_FREE_SPACE])
        cg.add(var.set_free_space_sensor(fs_sens))

    cg.add_define("USE_SD_CARD")
    cg.add_build_flag("-DUSE_ESP_IDF")













