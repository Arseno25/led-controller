idf_component_register(
    SRCS "mode_manager.c"
    INCLUDE_DIRS "include"
    REQUIRES config_manager animation_layer reactive_renderer reactive_matrix_renderer matrix_engine led_driver
)
