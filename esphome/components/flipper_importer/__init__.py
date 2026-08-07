import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import remote_transmitter, text_sensor, web_server_base
from esphome.components.web_server_base import CONF_WEB_SERVER_BASE_ID
from esphome.const import CONF_ID

CODEOWNERS = []
DEPENDENCIES = ["web_server"]
AUTO_LOAD = ["web_server_base"]

CONF_STORAGE_ID = "storage_id"
CONF_IR_TRANSMITTER_ID = "ir_transmitter_id"
CONF_RF_TRANSMITTER_ID = "rf_transmitter_id"
CONF_IR_STATUS_ID = "ir_status_id"
CONF_RF_STATUS_ID = "rf_status_id"
CONF_IR_SLOT_STATUS_IDS = "ir_slot_status_ids"
CONF_RF_SLOT_STATUS_IDS = "rf_slot_status_ids"

flash_comp_ns = cg.esphome_ns.namespace("flash_component")
FlashComp = flash_comp_ns.class_("Flash_comp", cg.Component)

flipper_importer_ns = cg.esphome_ns.namespace("flipper_importer")
FlipperImporter = flipper_importer_ns.class_("FlipperImporter", cg.Component)


def _validate_slot_sensor_counts(config):
    if len(config[CONF_IR_SLOT_STATUS_IDS]) != 10:
        raise cv.Invalid("ir_slot_status_ids must contain exactly 10 text sensors")
    if len(config[CONF_RF_SLOT_STATUS_IDS]) != 16:
        raise cv.Invalid("rf_slot_status_ids must contain exactly 16 text sensors")
    return config

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
        cv.GenerateID(): cv.declare_id(FlipperImporter),
        cv.GenerateID(CONF_WEB_SERVER_BASE_ID): cv.use_id(web_server_base.WebServerBase),
        cv.Required(CONF_STORAGE_ID): cv.use_id(FlashComp),
        cv.Required(CONF_IR_TRANSMITTER_ID): cv.use_id(
            remote_transmitter.RemoteTransmitterComponent
        ),
        cv.Required(CONF_RF_TRANSMITTER_ID): cv.use_id(
            remote_transmitter.RemoteTransmitterComponent
        ),
        cv.Required(CONF_IR_STATUS_ID): cv.use_id(text_sensor.TextSensor),
        cv.Required(CONF_RF_STATUS_ID): cv.use_id(text_sensor.TextSensor),
        cv.Required(CONF_IR_SLOT_STATUS_IDS): cv.ensure_list(
            cv.use_id(text_sensor.TextSensor)
        ),
        cv.Required(CONF_RF_SLOT_STATUS_IDS): cv.ensure_list(
            cv.use_id(text_sensor.TextSensor)
        ),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    _validate_slot_sensor_counts,
)


async def to_code(config):
    base = await cg.get_variable(config[CONF_WEB_SERVER_BASE_ID])
    storage = await cg.get_variable(config[CONF_STORAGE_ID])
    var = cg.new_Pvariable(config[CONF_ID], base, storage)
    await cg.register_component(var, config)

    ir_transmitter = await cg.get_variable(config[CONF_IR_TRANSMITTER_ID])
    rf_transmitter = await cg.get_variable(config[CONF_RF_TRANSMITTER_ID])
    ir_status = await cg.get_variable(config[CONF_IR_STATUS_ID])
    rf_status = await cg.get_variable(config[CONF_RF_STATUS_ID])

    cg.add(var.set_ir_transmitter(ir_transmitter))
    cg.add(var.set_rf_transmitter(rf_transmitter))
    cg.add(var.set_ir_status(ir_status))
    cg.add(var.set_rf_status(rf_status))

    for slot, sensor_id in enumerate(config[CONF_IR_SLOT_STATUS_IDS]):
        sensor = await cg.get_variable(sensor_id)
        cg.add(var.set_ir_slot_status(slot, sensor))
    for slot, sensor_id in enumerate(config[CONF_RF_SLOT_STATUS_IDS]):
        sensor = await cg.get_variable(sensor_id)
        cg.add(var.set_rf_slot_status(slot, sensor))
