import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.const import CONF_ID

DEPENDENCIES = ['esp32']

sd_mmc_card_ns = cg.esphome_ns.namespace('sd_mmc_card')
SDMMCCard = sd_mmc_card_ns.class_('SDMMCCard', cg.Component)

CONF_DATA3_PIN = 'data3_pin'
CONF_CMD_PIN = 'cmd_pin'
CONF_DATA0_PIN = 'data0_pin'
CONF_DATA1_PIN = 'data1_pin'
CONF_DATA2_PIN = 'data2_pin'
CONF_CLK_PIN = 'clk_pin'
CONF_MODE_1BIT = 'mode_1bit'

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(SDMMCCard),
    cv.Required(CONF_DATA3_PIN): pins.gpio_output_pin_schema,
    cv.Required(CONF_CMD_PIN): pins.gpio_output_pin_schema,
    cv.Required(CONF_DATA0_PIN): pins.gpio_output_pin_schema,
    cv.Required(CONF_DATA1_PIN): pins.gpio_output_pin_schema,
    cv.Required(CONF_DATA2_PIN): pins.gpio_output_pin_schema,
    cv.Required(CONF_CLK_PIN): pins.gpio_output_pin_schema,
    cv.Optional(CONF_MODE_1BIT, default=False): cv.boolean,
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    pins = [CONF_DATA3_PIN, CONF_CMD_PIN, CONF_DATA0_PIN, CONF_DATA1_PIN, CONF_DATA2_PIN, CONF_CLK_PIN]
    for pin in pins:
        pin_obj = await cg.gpio_pin_expression(config[pin])
        cg.add(getattr(var, f"set_{pin}")(pin_obj))

    cg.add(var.set_mode_1bit(config[CONF_MODE_1BIT]))







