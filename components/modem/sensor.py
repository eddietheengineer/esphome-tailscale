import esphome.codegen as cg
from esphome.components import sensor
from esphome.const import CONF_ID, STATE_CLASS_MEASUREMENT
import esphome.config_validation as cv

from . import ModemComponent

DEPENDENCIES = ["modem"]

CONF_RSSI_DBM = "rssi_dbm"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(ModemComponent),
        cv.Optional(CONF_RSSI_DBM): sensor.sensor_schema(
            unit_of_measurement="dBm",
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_ID])
    if rssi_config := config.get(CONF_RSSI_DBM):
        sens = await sensor.new_sensor(rssi_config)
        cg.add(hub.set_rssi_sensor(sens))
