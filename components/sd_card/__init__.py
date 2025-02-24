import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import spi
from esphome.const import CONF_ID

DEPENDENCIES = ['spi']
AUTO_LOAD = ['spi']

sd_card_ns = cg.esphome_ns.namespace('sd_card')
SDCard = sd_card_ns.class_('SDCard', cg.Component, spi.SPIDevice)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(SDCard),
}).extend(cv.COMPONENT_SCHEMA).extend(spi.spi_device_schema())

def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield spi.register_spi_device(var, config)


