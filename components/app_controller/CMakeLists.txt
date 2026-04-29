idf_component_register(
    SRCS "app_controller.c"
    INCLUDE_DIRS "include"
    REQUIRES config_manager led_driver animation_layer wifi_service web_server
             audio_input audio_processor reactive_engine
             mode_manager reactive_renderer reactive_matrix_renderer
             matrix_engine random_effect_manager palette_manager system_monitor
             display_manager
    PRIV_REQUIRES esp_timer
)
