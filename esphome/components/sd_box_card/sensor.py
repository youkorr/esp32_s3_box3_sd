import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_ID,
    CONF_SD_CARD_ID,
    DEVICE_CLASS_SD,
    STATE_CLASS_MEASUREMENT,
    UNIT_MEGABYTE,
    ICON_CARD_BULK,
)


DEPENDENCIES = ["sd_box_card"]

SDCardDiskUsageSensor = sd_box_card_ns.class_(
    "SDCardDiskUsageSensor", sensor.Sensor, cg.Component
)

CONF_TYPE = "type"
TYPE_FREE = "free"
TYPE_USED = "used"
TYPE_TOTAL = "total"

CONFIG_SCHEMA = sensor.sensor_schema(
    SDCardDiskUsageSensor,
    unit_of_measurement=UNIT_MEGABYTE,
    icon=ICON_CARD_BULK,
    accuracy_decimals=1,
    state_class=STATE_CLASS_MEASUREMENT,
).extend(
    {
        cv.GenerateID(CONF_SD_CARD_ID): cv.use_id(SDBoxCard),
        cv.Required(CONF_TYPE): cv.one_of(TYPE_FREE, TYPE_USED, TYPE_TOTAL, lower=True),
    }
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await sensor.register_sensor(var, config)
    
    parent = await cg.get_variable(config[CONF_SD_CARD_ID])
    cg.add(var.set_parent(parent))
    
    cg.add(var.set_type(config[CONF_TYPE]))
