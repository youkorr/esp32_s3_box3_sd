import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    DEVICE_CLASS_DATA_SIZE,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    UNIT_GIGABYTE,
)
from . import CONF_SD_BOX_CARD, CONF_FREE_SPACE, CONF_TOTAL_SPACE, CONF_USED_SPACE

DEPENDENCIES = ["sd_card"]

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_SD_BOX_CARD): cv.use_id(cg.Component),
        cv.Optional(CONF_TOTAL_SPACE): sensor.sensor_schema(
            unit_of_measurement=UNIT_GIGABYTE,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_DATA_SIZE,
            state_class=STATE_CLASS_MEASUREMENT,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(CONF_USED_SPACE): sensor.sensor_schema(
            unit_of_measurement=UNIT_GIGABYTE,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_DATA_SIZE,
            state_class=STATE_CLASS_MEASUREMENT,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(CONF_FREE_SPACE): sensor.sensor_schema(
            unit_of_measurement=UNIT_GIGABYTE,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_DATA_SIZE,
            state_class=STATE_CLASS_MEASUREMENT,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
    }
)

async def to_code(config):
    sd_card = await cg.get_variable(config[CONF_SD_BOX_CARD])
    
    if CONF_TOTAL_SPACE in config:
        sens = await sensor.new_sensor(config[CONF_TOTAL_SPACE])
        cg.add(sd_card.set_total_space_sensor(sens))
    
    if CONF_USED_SPACE in config:
        sens = await sensor.new_sensor(config[CONF_USED_SPACE])
        cg.add(sd_card.set_used_space_sensor(sens))
    
    if CONF_FREE_SPACE in config:
        sens = await sensor.new_sensor(config[CONF_FREE_SPACE])
        cg.add(sd_card.set_free_space_sensor(sens))
