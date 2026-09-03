import esphome.codegen as cg
from esphome.components import climate_ir

AUTO_LOAD = ["climate_ir"]

fujitsu_vertical_ns = cg.esphome_ns.namespace("fujitsu_vertical")
FujitsuVerticalClimate = fujitsu_vertical_ns.class_(
    "FujitsuVerticalClimate", climate_ir.ClimateIR
)

CONFIG_SCHEMA = climate_ir.climate_ir_with_receiver_schema(FujitsuVerticalClimate)


async def to_code(config):
    await climate_ir.new_climate_ir(config)
