idf_component_register(
    SRCS "animation_layer.c"
    INCLUDE_DIRS "include"
    REQUIRES led_driver config_manager
    PRIV_REQUIRES esp_timer
)
