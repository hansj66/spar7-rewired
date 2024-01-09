set (EXTRA_COMPONENT_DIRS "./components/modbus")

idf_component_register(
        SRCS 
            "main.c"
            "filesystem.c"
            "dtls.c"
            "z_funcs.c"
            "z_coap.c"
            "coap_client.c"
            "wifi.c"
            "comms.c"
        INCLUDE_DIRS 
            "."
            "F:/Espressif/frameworks/esp-idf-v5.1.1/components/freertos/FreeRTOS-Kernel/include/freertos"
        REQUIRES
            log
            spi_flash
            driver
            spiffs
            mbedtls
            bootloader_support
            esp_wifi
            lwip
    )

target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-format")

spiffs_create_partition_image(storage ../certs/chain FLASH_IN_PROJECT)