idf_component_register(
    SRCS "audio_processor.c"
    INCLUDE_DIRS "include"
    REQUIRES config_manager
)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-unused-variable)
