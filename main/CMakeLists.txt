FILE(GLOB SOURCES *.c)
idf_component_register(SRCS ${SOURCES}
                    INCLUDE_DIRS "."
                    EMBED_FILES server/index.html server/favicon.ico
                    REQUIRES nvs_flash esp_http_server esp_wifi
                              esp_netif esp_event efuse
                    PRIV_REQUIRES app_update)
