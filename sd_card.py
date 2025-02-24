import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import CONF_ID, UNIT_PERCENT, ICON_SD_CARD

DEPENDENCIES = ['esp32']

esp32_s3_box3_sd_ns = cg.esphome_ns.namespace('esp32_s3_box3_sd')
ESP32S3Box3SDCard = esp32_s3_box3_sd_ns.class_('ESP32S3Box3SDCard', cg.Component)

CONF_SPACE_USED = "space_used"
CONF_TOTAL_SPACE = "total_space"

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(ESP32S3Box3SDCard),
    cv.Optional(CONF_SPACE_USED): sensor.sensor_schema(
        unit_of_measurement=UNIT_PERCENT,
        icon=ICON_SD_CARD,
        accuracy_decimals=1
    ),
    cv.Optional(CONF_TOTAL_SPACE): sensor.sensor_schema(
        unit_of_measurement="MB",
        icon=ICON_SD_CARD,
        accuracy_decimals=0
    ),
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    if CONF_SPACE_USED in config:
        sens = await sensor.new_sensor(config[CONF_SPACE_USED])
        cg.add(var.set_space_used_sensor(sens))

    if CONF_TOTAL_SPACE in config:
        sens = await sensor.new_sensor(config[CONF_TOTAL_SPACE])
        cg.add(var.set_total_space_sensor(sens))
