set (EXTRA_COMPONENT_DIRS "./components/modbus")

idf_component_register(
        SRCS 
            "main.c"
        INCLUDE_DIRS 
            "."
            "C:/Espressif/frameworks/esp-idf-v5.1.1/components/freertos/FreeRTOS-Kernel/include/freertos"
        REQUIRES
            log
            spi_flash
    )

target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-format")