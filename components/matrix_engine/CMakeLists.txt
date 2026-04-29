idf_component_register(
    SRCS "matrix_engine.c"
    INCLUDE_DIRS "include"
    REQUIRES config_manager led_driver audio_processor esp_hw_support
)
