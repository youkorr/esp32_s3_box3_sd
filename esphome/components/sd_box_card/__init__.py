import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.const import CONF_ID

DEPENDENCIES = ['esp32']

sd_box_card_ns = cg.esphome_ns.namespace('sd_box_card')
SDBoxCard = sd_box_card_ns.class_('SDBoxCard', cg.Component)

CONF_MOSI_PIN = 'mosi_pin'
CONF_MISO_PIN = 'miso_pin'
CONF_CLK_PIN = 'clk_pin'
CONF_CS_PIN = 'cs_pin'

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(SDBoxCard),
    cv.Required(CONF_MOSI_PIN): pins.gpio_output_pin_schema,
    cv.Required(CONF_MISO_PIN): pins.gpio_input_pin_schema,
    cv.Required(CONF_CLK_PIN): pins.gpio_output_pin_schema,
    cv.Required(CONF_CS_PIN): pins.gpio_output_pin_schema,
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    mosi = await cg.gpio_pin_expression(config[CONF_MOSI_PIN])
    miso = await cg.gpio_pin_expression(config[CONF_MISO_PIN])
    clk = await cg.gpio_pin_expression(config[CONF_CLK_PIN])
    cs = await cg.gpio_pin_expression(config[CONF_CS_PIN])

    cg.add(var.set_mosi_pin(mosi))
    cg.add(var.set_miso_pin(miso))
    cg.add(var.set_clk_pin(clk))
    cg.add(var.set_cs_pin(cs))









