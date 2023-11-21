idf_component_register(SRCS camera_main.c code_wifi.c tcp_server.c
                        PRIV_INCLUDE_DIRS .
                        PRIV_REQUIRES nvs_flash
                        REQUIRES esp_https_server esp_wifi esp_timer spi_flash esp_psram)
                     
