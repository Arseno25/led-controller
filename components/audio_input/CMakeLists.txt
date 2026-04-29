idf_component_register(
    SRCS "audio_input.c"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_driver_i2s config_manager
)
