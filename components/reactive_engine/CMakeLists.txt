idf_component_register(
    SRCS "reactive_engine.c"
    INCLUDE_DIRS "include"
    REQUIRES audio_input audio_processor config_manager
)
