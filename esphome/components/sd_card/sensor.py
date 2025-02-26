import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    DEVICE_CLASS_DATA_SIZE,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    UNIT_BYTE,
    ICON_SD_CARD,
    CONF_TYPE,
)

DEPENDENCIES = ["sd_card"]

CONF_SD_CARD_ID = "sd_card_id"
CONF_TOTAL_SPACE = "total_space"
CONF_USED_SPACE = "used_space"
CONF_FREE_SPACE = "free_space"

TYPES = [CONF_TOTAL_SPACE, CONF_USED_SPACE, CONF_FREE_SPACE]

sd_card_ns = cg.esphome_ns.namespace("sd_card")
SDCard = sd_card_ns.class_("SDCard", cg.Component)

BASE_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_BYTE,
    icon=ICON_SD_CARD,
    accuracy_decimals=2,
    device_class=DEVICE_CLASS_DATA_SIZE,
    state_class=STATE_CLASS_MEASUREMENT,
    entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
).extend({
    cv.GenerateID(CONF_SD_CARD_ID): cv.use_id(SDCard),
})

CONFIG_SCHEMA = cv.Schema({
    cv.Required(CONF_TYPE): cv.one_of(*TYPES, lower=True),
}).extend(BASE_SCHEMA)

async def to_code(config):
    sd_card = await cg.get_variable(config[CONF_SD_CARD_ID])
    var = await sensor.new_sensor(config)
    func = getattr(sd_card, f"set_{config[CONF_TYPE]}_sensor")
    cg.add(func(var))


