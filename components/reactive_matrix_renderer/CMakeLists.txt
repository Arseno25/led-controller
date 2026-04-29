idf_component_register(
    SRCS "reactive_matrix_renderer.c"
    INCLUDE_DIRS "include"
    REQUIRES config_manager matrix_engine audio_processor led_driver
)
