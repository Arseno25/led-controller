idf_component_register(
    SRCS "../../src/display/DisplayManager.cpp"
    INCLUDE_DIRS "../../src/display"
    REQUIRES config_manager audio_processor system_monitor
    PRIV_REQUIRES esp_timer esp_driver_spi esp_driver_gpio
)
