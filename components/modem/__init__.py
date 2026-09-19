import os
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.components.esp32 import (
    add_idf_component,
    add_idf_sdkconfig_option,
)

CODEOWNERS = ["@esphome-tailscale"]
DEPENDENCIES = ["esp32"]
# Load the network component (esp_netif_init) the same way wifi/ethernet do.
AUTO_LOAD = ["network"]

CONF_APN = "apn"
CONF_SIM_PIN = "sim_pin"
CONF_PPP_USER = "ppp_user"
CONF_PPP_PASS = "ppp_pass"
CONF_TX_PIN = "tx_pin"
CONF_RX_PIN = "rx_pin"
CONF_PWRKEY_PIN = "pwrkey_pin"
CONF_DTR_PIN = "dtr_pin"

modem_ns = cg.esphome_ns.namespace("modem")
ModemComponent = modem_ns.class_("ModemComponent", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(ModemComponent),
        cv.Optional(CONF_APN, default=""): cv.string,
        cv.Optional(CONF_SIM_PIN, default=""): cv.string,
        cv.Optional(CONF_PPP_USER, default=""): cv.string,
        cv.Optional(CONF_PPP_PASS, default=""): cv.sensitive(),
        # Modem UART / power pins. Board-specific and REQUIRED: the microlink
        # Kconfig board preset (TX=11/RX=10/PWRKEY=18/DTR=9) does not match
        # the LILYGO T-SIM7670G-S3 Standard SKU (TX=4/RX=5/PWRKEY=46/DTR=7),
        # so there is no safe default — set these to match your board.
        cv.Required(CONF_TX_PIN): cv.int_range(min=0, max=48),
        cv.Required(CONF_RX_PIN): cv.int_range(min=0, max=48),
        cv.Required(CONF_PWRKEY_PIN): cv.int_range(min=-1, max=48),
        cv.Required(CONF_DTR_PIN): cv.int_range(min=-1, max=48),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    cg.add_define("USE_MODEM")
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_apn(config[CONF_APN]))
    cg.add(var.set_sim_pin(config[CONF_SIM_PIN]))
    cg.add(var.set_ppp_user(config[CONF_PPP_USER]))
    cg.add(var.set_ppp_pass(config[CONF_PPP_PASS]))
    cg.add(var.set_tx_pin(config[CONF_TX_PIN]))
    cg.add(var.set_rx_pin(config[CONF_RX_PIN]))
    cg.add(var.set_pwrkey_pin(config[CONF_PWRKEY_PIN]))
    cg.add(var.set_dtr_pin(config[CONF_DTR_PIN]))

    # Enable the microlink cellular driver + LILYGO T-SIM7670G-S3 board preset
    # + the ESP-IDF PPP netif driver. (The `tailscale` component sets these too
    # when its own `cellular:` block is enabled; in the MQTT/hybrid config the
    # tailscale component runs with cellular disabled, so this is the sole
    # provider of the PPP uplink.)
    add_idf_sdkconfig_option("CONFIG_ML_ENABLE_CELLULAR", True)
    add_idf_sdkconfig_option("CONFIG_ML_BOARD_LILYGO_T_SIM7670G", True)
    add_idf_sdkconfig_option("CONFIG_PPP_SUPPORT", True)
    # PWRKEY / DTR are compile-time Kconfig (the driver reads
    # CONFIG_ML_CELLULAR_PWRKEY_PIN / CONFIG_ML_CELLULAR_DTR_PIN); write them
    # unconditionally. The UART TX/RX pins are passed to the driver at runtime
    # via ml_cellular_config_t (see modem_component.cpp) — the driver does not
    # read CONFIG_ML_CELLULAR_TX_PIN/RX_PIN, so no Kconfig for those.
    add_idf_sdkconfig_option("CONFIG_ML_CELLULAR_PWRKEY_PIN", config[CONF_PWRKEY_PIN])
    add_idf_sdkconfig_option("CONFIG_ML_CELLULAR_DTR_PIN", config[CONF_DTR_PIN])

    # Wire the microlink ESP-IDF component into the build. add_idf_component is
    # name-keyed, so if the `tailscale` component also adds it (for the
    # on-demand OTA node) the second call is idempotent — same path, no conflict.
    this_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.abspath(os.path.join(this_dir, "..", ".."))
    ml_base = os.path.join(project_root, "microlink", "components", "microlink").replace("\\", "/")
    add_idf_component(name="wireguard_lwip", path=f"{ml_base}/components/wireguard_lwip")
    add_idf_component(name="microlink", path=ml_base)
