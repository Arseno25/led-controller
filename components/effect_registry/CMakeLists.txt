idf_component_register(
    SRCS "effect_registry.c"
    INCLUDE_DIRS "include"
    REQUIRES config_manager
    PRIV_REQUIRES esp_hw_support
)
