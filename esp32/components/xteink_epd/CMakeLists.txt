idf_component_register(
    SRCS "src/bus/EpdBus.cpp" "src/probe.cpp"
         "src/driver/Ssd1677Driver.cpp" "src/driver/Uc8179Driver.cpp"
         "src/driver/Uc8279X4Driver.cpp"
    INCLUDE_DIRS "include" "compat" "src"
    REQUIRES esp_driver_spi esp_driver_gpio esp_timer esp_rom)
target_compile_definitions(${COMPONENT_LIB} PUBLIC FREEINK_DEVICE_X4PRO=1 BOARD_HAS_PSRAM=1)
