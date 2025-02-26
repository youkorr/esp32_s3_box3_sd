import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, text_sensor
from esphome.const import (
    CONF_ID,
    CONF_TYPE,
    DEVICE_CLASS_DATA_SIZE,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    UNIT_GIGABYTE,
)

DEPENDENCIES = ["esp32"]
CODEOWNERS = ["@votreadressee"]

CONF_SD_BOX_CARD = "sd_box_card"
CONF_CMD_PIN = "cmd_pin"
CONF_DATA0_PIN = "data0_pin"
CONF_DATA1_PIN = "data1_pin"
CONF_DATA2_PIN = "data2_pin"
CONF_DATA3_PIN = "data3_pin"
CONF_MODE_1BIT = "mode_1bit"
CONF_CARD_TYPE = "card_type"
CONF_TOTAL_SPACE = "total_space"
CONF_USED_SPACE = "used_space"
CONF_FREE_SPACE = "free_space"

sd_card_ns = cg.esphome_ns.namespace("sd_card")
SDCard = sd_card_ns.class_("SDCard", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_SD_CARD): cv.Schema(
            {
                cv.GenerateID(): cv.declare_id(SDCard),
                cv.Required(CONF_CLK_PIN): cv.gpio_pin,
                cv.Required(CONF_CMD_PIN): cv.gpio_pin,
                cv.Required(CONF_DATA0_PIN): cv.gpio_pin,
                cv.Optional(CONF_DATA1_PIN): cv.gpio_pin,
                cv.Optional(CONF_DATA2_PIN): cv.gpio_pin,
                cv.Optional(CONF_DATA3_PIN): cv.gpio_pin,
                cv.Optional(CONF_MODE_1BIT, default=False): cv.boolean,
                cv.Optional(CONF_CARD_TYPE): cv.use_id(text_sensor.TextSensor),
                cv.Optional(CONF_TOTAL_SPACE): cv.use_id(sensor.Sensor),
                cv.Optional(CONF_USED_SPACE): cv.use_id(sensor.Sensor),
                cv.Optional(CONF_FREE_SPACE): cv.use_id(sensor.Sensor),
            }
        )
    }
)

async def to_code(config):
    sd_config = config[CONF_SD_BOX_CARD]
    var = cg.new_Pvariable(sd_config[CONF_ID])
    await cg.register_component(var, sd_config)

    cg.add(var.set_clk_pin(sd_config[CONF_CLK_PIN]))
    cg.add(var.set_cmd_pin(sd_config[CONF_CMD_PIN]))
    cg.add(var.set_data0_pin(sd_config[CONF_DATA0_PIN]))
    
    if CONF_DATA1_PIN in sd_config:
        cg.add(var.set_data1_pin(sd_config[CONF_DATA1_PIN]))
    if CONF_DATA2_PIN in sd_config:
        cg.add(var.set_data2_pin(sd_config[CONF_DATA2_PIN]))
    if CONF_DATA3_PIN in sd_config:
        cg.add(var.set_data3_pin(sd_config[CONF_DATA3_PIN]))
    
    cg.add(var.set_mode_1bit(sd_config[CONF_MODE_1BIT]))

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
    # Spécifier d'utiliser le mode IDF au lieu d'Arduino
    cg.add_build_flag("-DUSE_ESP_IDF")





