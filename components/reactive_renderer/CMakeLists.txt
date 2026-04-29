idf_component_register(
    SRCS "reactive_renderer.c"
    INCLUDE_DIRS "include"
    REQUIRES config_manager audio_processor led_driver esp_hw_support
)
