import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import CONF_ID

from . import sd_card  # Import the sd_card module

DEPENDENCIES = ['esp32']
AUTO_LOAD = ['sensor']

CONF_ESP32_S3_BOX3_SD = 'esp32_s3_box3_sd'

CONFIG_SCHEMA = cv.Schema({
    cv.Optional(CONF_ESP32_S3_BOX3_SD): sd_card.CONFIG_SCHEMA,
})

async def to_code(config):
    if CONF_ESP32_S3_BOX3_SD in config:
        conf = config[CONF_ESP32_S3_BOX3_SD]
        cg.include("sd_card.h")  # Include the header file
        var = cg.NewPvariable(conf[CONF_ID])
        await cg.RegisterComponent(var, conf)

        # Register sensors
        if "space_used" in conf:
            sens = await sensor.new_sensor(conf["space_used"])
            cg.add(var.set_space_used_sensor(sens))
        if "total_space" in conf:
            sens = await sensor.new_sensor(conf["total_space"])
            cg.add(var.set_total_space_sensor(sens))




