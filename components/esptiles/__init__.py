import esphome.codegen as cg
import esphome.config_validation as cv

from esphome.config_helpers import filter_source_files_from_platform
from esphome.const import CONF_ID, PlatformFramework
from esphome.core import CORE

CONF_URL_TEMPLATE = "url_template"

esptiles_ns = cg.esphome_ns.namespace("esptiles")

if CORE.is_esp32 and CORE.using_esp_idf:
    esptiles = esptiles_ns.class_("EspTiles_EspIdf", cg.Component)
    DEPENDENCIES = ["wifi"]
elif CORE.is_esp32 and CORE.using_arduino:
    esptiles = esptiles_ns.class_("EspTilesArduino", cg.Component)
    DEPENDENCIES = ["wifi"]
elif CORE.is_host:
    esptiles = esptiles_ns.class_("EspTilesx86", cg.Component)
else:
    raise Exception("Unsupported platform for SignalK component")

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(esptiles),
    cv.Required(CONF_URL_TEMPLATE): cv.string,
})


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID], config[CONF_URL_TEMPLATE])
    await cg.register_component(var, config)

    cg.add(var.set_setup_priority(100))
    cg.add_library("bblanchon/ArduinoJson", "7.4.2")
    if CORE.is_esp32 and CORE.using_esp_idf:
        cg.add_library(
            "esp_websocket_client",
            None,
            repository="https://components.espressif.com/api/downloads/?object_type=component&object_id=dbc87006-9a4b-45e6-a6ab-b286174cb413",
        )
    if CORE.is_esp32 and CORE.using_arduino:
        cg.add_library("NetworkClientSecure", None)
        cg.add_library("HTTPClient", None)
        # cg.add_library("links2004/WebSockets", "2.6.1")
        # cg.add_library("gilmaimon/ArduinoWebsockets", "0.5.4")
    if CORE.is_host:
        cg.add_build_flag("-I/usr/local/include")
        cg.add_build_flag("-L/usr/local/lib")
        cg.add_build_flag("-lixwebsocket")
        cg.add_build_flag("-lz")
        cg.add_build_flag("-lssl")
        cg.add_build_flag("-lcrypto")
        
        
        
        cg.add_platformio_option("build_type", "debug")

        
FILTER_SOURCE_FILES = filter_source_files_from_platform(
    {
        # "signalk_client_arduino_esp32.cpp": {PlatformFramework.ESP32_ARDUINO},
        # "esptiles_espidf.cpp": {PlatformFramework.ESP32_IDF},
        "esptiles_x86.cpp": {PlatformFramework.HOST_NATIVE},
    }
)
