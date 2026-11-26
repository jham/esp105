idf_component_register(SRC_DIRS "."
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES cn105 esp_driver_gpio esp_driver_ledc esp_timer nvs_flash)
target_compile_options(${COMPONENT_LIB} PRIVATE "-DCHIP_HAVE_CONFIG_H")
