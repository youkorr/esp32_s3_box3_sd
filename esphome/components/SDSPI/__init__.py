import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation, pins
from esphome.const import (
    CONF_ID,
    CONF_DATA,
    CONF_PATH,
    CONF_CLK_PIN,
    CONF_INPUT,
    CONF_OUTPUT,
)
from esphome.core import CORE

# Déclaration des constantes
CONF_SD_MMC_CARD_ID = "sd_mmc_card_id"
CONF_CMD_PIN = "cmd_pin"
CONF_DATA0_PIN = "data0_pin"
CONF_DATA1_PIN = "data1_pin"
CONF_DATA2_PIN = "data2_pin"
CONF_DATA3_PIN = "data3_pin"
CONF_MODE_1BIT = "mode_1bit"
CONF_MAX_FREQ_KHZ = "max_freq_khz"
CONF_MOUNT_POINT = "mount_point"

# Déclaration du namespace et de la classe du composant
sdpi_ns = cg.esphome_ns.namespace("SDPI")
SDPIComponent = sdpi_ns.class_("SDPIComponent", cg.Component)

# Actions (si nécessaire)
SDPIWriteFileAction = sdpi_ns.class_("SDPIWriteFileAction", automation.Action)
SDPIAppendFileAction = sdpi_ns.class_("SDPIAppendFileAction", automation.Action)
SDPICreateDirectoryAction = sdpi_ns.class_("SDPICreateDirectoryAction", automation.Action)
SDPIRemoveDirectoryAction = sdpi_ns.class_("SDPIRemoveDirectoryAction", automation.Action)
SDPIDeleteFileAction = sdpi_ns.class_("SDPIDeleteFileAction", automation.Action)

# Validation des données brutes
def validate_raw_data(value):
    if isinstance(value, str):
        return value.encode("utf-8")
    if isinstance(value, list):
        return cv.Schema([cv.hex_uint8_t])(value)
    raise cv.Invalid(
        "data must either be a string wrapped in quotes or a list of bytes"
    )

# Schéma de configuration YAML
CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(SDPIComponent),
    cv.Required(CONF_CLK_PIN): pins.internal_gpio_output_pin_number,
    cv.Required(CONF_CMD_PIN): pins.internal_gpio_output_pin_number,
    cv.Required(CONF_DATA0_PIN): pins.internal_gpio_pin_number({CONF_OUTPUT: True, CONF_INPUT: True}),
    cv.Optional(CONF_DATA1_PIN): pins.internal_gpio_pin_number({CONF_OUTPUT: True, CONF_INPUT: True}),
    cv.Optional(CONF_DATA2_PIN): pins.internal_gpio_pin_number({CONF_OUTPUT: True, CONF_INPUT: True}),
    cv.Optional(CONF_DATA3_PIN): pins.internal_gpio_pin_number({CONF_OUTPUT: True, CONF_INPUT: True}),
    cv.Optional(CONF_MODE_1BIT, default=False): cv.boolean,
    cv.Optional(CONF_MAX_FREQ_KHZ, default=20000): cv.int_range(min=400, max=20000),
    cv.Optional(CONF_MOUNT_POINT, default="/sdcard"): cv.string,
}).extend(cv.COMPONENT_SCHEMA)

# Fonction pour générer le code C++
async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_mode_1bit(config[CONF_MODE_1BIT]))
    cg.add(var.set_max_freq_khz(config[CONF_MAX_FREQ_KHZ]))
    cg.add(var.set_mount_point(config[CONF_MOUNT_POINT]))

    cg.add(var.set_clk_pin(config[CONF_CLK_PIN]))
    cg.add(var.set_cmd_pin(config[CONF_CMD_PIN]))
    cg.add(var.set_data0_pin(config[CONF_DATA0_PIN]))

    if not config[CONF_MODE_1BIT]:
        cg.add(var.set_data1_pin(config[CONF_DATA1_PIN]))
        cg.add(var.set_data2_pin(config[CONF_DATA2_PIN]))
        cg.add(var.set_data3_pin(config[CONF_DATA3_PIN]))

    # Ajout des dépendances nécessaires
    if CORE.using_arduino:
        cg.add_library("FS", None)
        cg.add_library("SD", None)

    # Montage automatique de la carte SD
    cg.add_define("USE_SD_MMC")
    cg.add(
        f"""
        esp_vfs_fat_sdmmc_mount_config_t mount_config = {{
            .format_if_mount_failed = false,
            .max_files = 5,
            .allocation_unit_size = 16 * 1024
        }};
        sdmmc_card_t *card;
        esp_err_t ret = esp_vfs_fat_sdmmc_mount(
            {config[CONF_MOUNT_POINT]},
            &host,
            &slot_config,
            &mount_config,
            &card
        );
        if (ret != ESP_OK) {{
            ESP_LOGE("SDPI", "Failed to mount SD card: %s", esp_err_to_name(ret));
            return;
        }}
        ESP_LOGI("SDPI", "SD card mounted at %s", {config[CONF_MOUNT_POINT]});
        """
    )

# Schéma pour les actions (si nécessaire)
SDPI_PATH_ACTION_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.use_id(SDPIComponent),
    cv.Required(CONF_PATH): cv.templatable(cv.string_strict),
})

SDPI_WRITE_FILE_ACTION_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.use_id(SDPIComponent),
    cv.Required(CONF_PATH): cv.templatable(cv.string_strict),
    cv.Required(CONF_DATA): cv.templatable(validate_raw_data),
}).extend(SDPI_PATH_ACTION_SCHEMA)

@automation.register_action(
    "SDPI.write_file", SDPIWriteFileAction, SDPI_WRITE_FILE_ACTION_SCHEMA
)
async def sdpi_write_file_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    path_ = await cg.templatable(config[CONF_PATH], args, cg.std_string)
    data_ = await cg.templatable(config[CONF_DATA], args, cg.std_vector.template(cg.uint8))
    cg.add(var.set_path(path_))
    cg.add(var.set_data(data_))
    return var
