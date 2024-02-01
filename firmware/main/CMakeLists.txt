set (EXTRA_COMPONENT_DIRS "./components/modbus")

idf_component_register(
        SRCS 
            "main.c"
            "filesystem.c"
           
           
           
           
            "wifi.c"
           
           
        INCLUDE_DIRS 
            "."
            "F:/Espressif/frameworks/esp-idf-v5.1.1/components/freertos/FreeRTOS-Kernel/include/freertos"
        REQUIRES
            log
            spi_flash
            driver
            spiffs
            bootloader_support
            esp_wifi
            lwip
            nvs_flash
            settings_spar7
            console_spar7
            hal_spar7
            dtls
    )

target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-format")

spiffs_create_partition_image(storage ../certs FLASH_IN_PROJECT)