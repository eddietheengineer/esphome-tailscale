import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv

from . import ModemComponent

DEPENDENCIES = ["modem"]

CONF_DATA_CONNECTED = "data_connected"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(ModemComponent),
        cv.Optional(CONF_DATA_CONNECTED): binary_sensor.binary_sensor_schema(
            icon="mdi:signal-cellular-connected",
        ),
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_ID])
    if data_config := config.get(CONF_DATA_CONNECTED):
        bs = await binary_sensor.new_binary_sensor(data_config)
        cg.add(hub.set_data_connected_sensor(bs))
