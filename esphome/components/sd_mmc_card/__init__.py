import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, binary_sensor
from esphome.const import CONF_ID, CONF_NAME
from esphome.components import sd_mmc_card

# from . import sd_card #THIS LINE IS COMMENTED

DEPENDENCIES = ['esp32']
AUTO_LOAD = ['sensor', 'binary_sensor']

CONF_ESP32_S3_BOX3_SD = 'sd_mmc_card' #MODIFIED LINE

CONFIG_SCHEMA = cv.Schema({
    cv.Optional(CONF_ESP32_S3_BOX3_SD): sd_mmc_card.CONFIG_SCHEMA, #MODIFIED LINE
})

async def to_code(config):
    if CONF_ESP32_S3_BOX3_SD in config:
        conf = config[CONF_ESP32_S3_BOX3_SD]
        cg.include("sd_mmc_card.h") #MODIFIED LINE
        var = cg.NewPvariable(conf[CONF_ID])
        await cg.RegisterComponent(var, conf)

        # if "space_used" in conf:
        #     sens = await sensor.new_sensor(conf["space_used"])
        #     cg.add(var.set_space_used_sensor(sens))

        # if "total_space" in conf:
        #     sens = await sensor.new_sensor(conf["total_space"])
        #     cg.add(var.set_total_space_sensor(sens))

