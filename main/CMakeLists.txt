set(requires "")
if(${IDF_TARGET} STREQUAL "linux")
    list(APPEND requires esp_stubs esp-tls esp_http_client protocol_examples_common nvs_flash)
endif()
idf_component_register(SRCS 
    esp-dallas-test.cpp dallas.cpp one-wire.cpp
    INCLUDE_DIRS "."
    REQUIRES ${requires}
)
