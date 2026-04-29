idf_component_register(
    SRCS "led_driver.c"
    INCLUDE_DIRS "include"
    REQUIRES driver config_manager
    PRIV_REQUIRES esp_driver_rmt led_strip
)
