import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, binary_sensor
from esphome.const import CONF_ID

DEPENDENCIES = ['esp32']
AUTO_LOAD = ['sensor']

CONF_SD_MMC_CARD = 'sd_mmc_card'

SD_MMC_CARD_SCHEMA = cv.Schema({  # Define the schema here
    cv.GenerateID(): cv.declare_id(cg.Component),
})

CONFIG_SCHEMA = cv.Schema({
    cv.Optional(CONF_SD_MMC_CARD): SD_MMC_CARD_SCHEMA,  # Use the schema here
})

async def to_code(config):
    if CONF_SD_MMC_CARD in config:
        conf = config[CONF_SD_MMC_CARD]
        cg.include("sd_mmc_card.h")
        var = cg.NewPvariable(conf[CONF_ID])
        await cg.RegisterComponent(var, conf)


