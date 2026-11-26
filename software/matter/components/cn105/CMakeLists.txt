idf_component_register(SRCS "cn105_protocol.cpp" "cn105.cpp"
                       INCLUDE_DIRS "include"
                       PRIV_REQUIRES esp_driver_uart freertos)
