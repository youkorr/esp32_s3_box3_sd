import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import CONF_ID, CONF_SD_CARD_ID, ICON_SD




SDCardInfoTextSensor = sd_box_card_ns.class_(
    "SDCardInfoTextSensor", text_sensor.TextSensor, cg.Component
)

CONF_INFO_TYPE = "info_type"
INFO_TYPE_CARD_TYPE = "card_type"

CONFIG_SCHEMA = text_sensor.TEXT_SENSOR_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(SDCardInfoTextSensor),
        cv.GenerateID(CONF_SD_CARD_ID): cv.use_id(SDBoxCard),
        cv.Required(CONF_INFO_TYPE): cv.one_of(INFO_TYPE_CARD_TYPE, lower=True),
    }
).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await text_sensor.register_text_sensor(var, config)
    
    parent = await cg.get_variable(config[CONF_SD_CARD_ID])
    cg.add(var.set_parent(parent))
    
    cg.add(var.set_info_type(config[CONF_INFO_TYPE]))
