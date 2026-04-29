idf_component_register(
    SRCS "web_server.c"
    INCLUDE_DIRS "include"
    EMBED_TXTFILES "web/index.html"
    REQUIRES esp_http_server json config_manager led_driver audio_processor
             palette_manager effect_registry system_monitor
)
