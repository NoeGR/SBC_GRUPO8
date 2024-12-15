idf_component_register(SRCS "respuestas_bot.c" "conexion_mqtt_thingsboard.c" "conexion_http_telegram.c" "display.c" "ADS1115.c" "dht.c" "nvs.c" "wifi.c" "main.c" 
                    INCLUDE_DIRS "."
                    EMBED_TXTFILES telegram_certificate.pem)