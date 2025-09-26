idf_component_register(SRC_DIRS led utils ws2812 .
                       INCLUDE_DIRS led utils ws2812 .
                       REQUIRES sw_timer rmt)

target_include_directories(
    ${COMPONENT_LIB} PRIVATE ${COMPONENT_INCLUDES}
)
