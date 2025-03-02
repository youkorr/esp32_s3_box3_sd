import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import sensor, text_sensor
from esphome.const import (
    CONF_ID,
    CONF_DATA_PIN,
    CONF_CLK_PIN,
    CONF_MODE,
    STATE_CLASS_MEASUREMENT,
    UNIT_MEGABYTE,
    ICON_CARD_BULK,
    ICON_SD,
)

# If you need a specific SD device class, define it here:
DEVICE_CLASS_SD = "sd"  # or whatever value you deem appropriate


CODEOWNERS = ["@votre_nom"]
DEPENDENCIES = []

sd_box_card_ns = cg.esphome_ns.namespace("sd_box_card")
SDBoxCard = sd_box_card_ns.class_("SDBoxCard", cg.Component)

CONF_SD_BOX_CARD = "sd_box_card"
CONF_DATA3_PIN = "data3_pin"
CONF_CMD_PIN = "cmd_pin"
CONF_DATA0_PIN = "data0_pin"
CONF_DATA1_PIN = "data1_pin"
CONF_DATA2_PIN = "data2_pin"
CONF_MODE_1BIT = "mode_1bit"

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(SDBoxCard),
    cv.Required(CONF_DATA3_PIN): pins.gpio_input_pin_schema,
    cv.Required(CONF_CMD_PIN): pins.gpio_input_pin_schema,
    cv.Required(CONF_DATA0_PIN): pins.gpio_input_pin_schema,
    cv.Required(CONF_DATA1_PIN): pins.gpio_input_pin_schema,
    cv.Required(CONF_DATA2_PIN): pins.gpio_input_pin_schema,
    cv.Required(CONF_CLK_PIN): pins.gpio_input_pin_schema,
    cv.Optional(CONF_MODE_1BIT, default=False): cv.boolean,
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    
    data3_pin = await cg.gpio_pin_expression(config[CONF_DATA3_PIN])
    cg.add(var.set_data3_pin(data3_pin))
    
    cmd_pin = await cg.gpio_pin_expression(config[CONF_CMD_PIN])
    cg.add(var.set_cmd_pin(cmd_pin))
    
    data0_pin = await cg.gpio_pin_expression(config[CONF_DATA0_PIN])
    cg.add(var.set_data0_pin(data0_pin))
    
    data1_pin = await cg.gpio_pin_expression(config[CONF_DATA1_PIN])
    cg.add(var.set_data1_pin(data1_pin))
    
    data2_pin = await cg.gpio_pin_expression(config[CONF_DATA2_PIN])
    cg.add(var.set_data2_pin(data2_pin))
    
    clk_pin = await cg.gpio_pin_expression(config[CONF_CLK_PIN])
    cg.add(var.set_clk_pin(clk_pin))
    
    cg.add(var.set_mode_1bit(config[CONF_MODE_1BIT]))









