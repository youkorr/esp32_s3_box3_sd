import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import ENTITY_CATEGORY_DIAGNOSTIC
from . import CONF_SD_BOX_CARD, CONF_CARD_TYPE

DEPENDENCIES = ["sd_card"]

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_SD_BOX_CARD): cv.use_id(cg.Component),
        cv.Optional(CONF_CARD_TYPE): text_sensor.text_sensor_schema(
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
    }
)

async def to_code(config):
    sd_card = await cg.get_variable(config[CONF_SD_BOX_CARD])
    
    if CONF_CARD_TYPE in config:
        sens = await text_sensor.new_text_sensor(config[CONF_CARD_TYPE])
        cg.add(sd_card.set_card_type_sensor(sens))
