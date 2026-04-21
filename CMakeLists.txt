idf_component_register(
    SRCS "src/uart_bootloader.c"
         "src/uart_bootloader_err.c"
         "src/uart_bootloader_transport_layer.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "src"
    REQUIRES intelhex
    PRIV_REQUIRES driver
                  esp_driver_uart
                  mlx_chip
                  mlx_crc
)
