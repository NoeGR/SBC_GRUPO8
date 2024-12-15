#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "nvs_flash.h"

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

#define CONFIG_NAMESPACE "wifi_config"

static const char *TAG_AP = "WiFi_AP";
static const char *TAG_STA = "WiFi_STA";
static const char *TAG_WEB = "WebServer";

static EventGroupHandle_t wifi_event_group;
static int retry_count = 0;
#define MAX_RETRY 5  // Número máximo de intentos para el modo STA
#define ESP_WIFI_AP_PASSWD "12345678"

/* Prototipos de funciones */
void start_web_server(void);

/* Handlers de eventos Wi-Fi */
static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG_STA, "Conectando...");
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (retry_count < MAX_RETRY) {
            retry_count++;
            ESP_LOGI(TAG_STA, "Reconectando... intento %d/%d", retry_count, MAX_RETRY);
            esp_wifi_connect();
        } else {
            ESP_LOGE(TAG_STA, "No se pudo conectar.");
            xEventGroupSetBits(wifi_event_group, WIFI_FAIL_BIT);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG_STA, "Conectado! IP: " IPSTR, IP2STR(&event->ip_info.ip));
        retry_count = 0;
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

/* Inicializar modo AP */
void start_softap() {
    ESP_LOGI(TAG_AP, "Iniciando modo AP...");

    esp_netif_t *netif_ap = esp_netif_create_default_wifi_ap();
    wifi_config_t wifi_ap_config = {
        .ap = {
            .ssid = "ESP32_AP",
            .password = ESP_WIFI_AP_PASSWD,
            .ssid_len = strlen("ESP32_AP"),
            .authmode = WIFI_AUTH_WPA2_PSK,
            .max_connection = 4,
        },
    };

    if (strlen(ESP_WIFI_AP_PASSWD) == 0) {
        wifi_ap_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_ap_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    start_web_server();
}

/* Inicializar modo STA */
void start_station(const char *ssid, const char *password) {
    ESP_LOGI(TAG_STA, "Iniciando modo STA con SSID: %s", ssid);

    esp_netif_t *netif_sta = esp_netif_create_default_wifi_sta();
    wifi_config_t wifi_sta_config = {
        .sta = {
            .scan_method = WIFI_ALL_CHANNEL_SCAN,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };

    strlcpy((char *)wifi_sta_config.sta.ssid, ssid, sizeof(wifi_sta_config.sta.ssid));
    strlcpy((char *)wifi_sta_config.sta.password, password, sizeof(wifi_sta_config.sta.password));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_sta_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

/* Servidor web: Handler GET */
static esp_err_t get_handler(httpd_req_t *req) {
    const char *response = "<html><body><h1>Configurar Wi-Fi</h1>"
                           "<form action='/config' method='POST'>"
                           "SSID: <input type='text' name='ssid'><br>"
                           "Password: <input type='password' name='password'><br>"
                           "<input type='submit' value='Conectar'>"
                           "</form></body></html>";
    httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

/* Servidor web: Handler POST */
static esp_err_t post_handler(httpd_req_t *req) {
    char buffer[100];
    int len = httpd_req_recv(req, buffer, sizeof(buffer));
    if (len <= 0) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    buffer[len] = '\0';

    char ssid[32], password[64];
    sscanf(buffer, "ssid=%31[^&]&password=%63s", ssid, password);

    ESP_LOGI(TAG_WEB, "Recibido: SSID=%s, Password=%s", ssid, password);

    // Guardar en NVS
    nvs_handle_t nvs_handle;
    ESP_ERROR_CHECK(nvs_open(CONFIG_NAMESPACE, NVS_READWRITE, &nvs_handle));
    ESP_ERROR_CHECK(nvs_set_str(nvs_handle, "ssid", ssid));
    ESP_ERROR_CHECK(nvs_set_str(nvs_handle, "password", password));
    ESP_ERROR_CHECK(nvs_commit(nvs_handle));
    nvs_close(nvs_handle);

    httpd_resp_send(req, "Configuracion guardada. Reiniciando...", HTTPD_RESP_USE_STRLEN);
    vTaskDelay(pdMS_TO_TICKS(2000));
    esp_restart();
    return ESP_OK;
}

/* Inicializar servidor web */
void start_web_server() {
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t get_uri = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = get_handler,
        };
        httpd_register_uri_handler(server, &get_uri);

        httpd_uri_t post_uri = {
            .uri = "/config",
            .method = HTTP_POST,
            .handler = post_handler,
        };
        httpd_register_uri_handler(server, &post_uri);
    }
}

/* Configuración inicial */
void init_wifi_system() {
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Leer credenciales desde NVS
    nvs_handle_t nvs_handle;
    char ssid[32], password[64];
    size_t ssid_len = sizeof(ssid), password_len = sizeof(password);

    esp_err_t err = nvs_open(CONFIG_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err == ESP_OK) {
        err = nvs_get_str(nvs_handle, "ssid", ssid, &ssid_len);
        err |= nvs_get_str(nvs_handle, "password", password, &password_len);
        nvs_close(nvs_handle);
    }

    if (err == ESP_OK) {
        // Intentar conectar con las credenciales almacenadas
        ESP_LOGI(TAG_STA, "Intentando conectar con SSID: %s", ssid);
        start_station(ssid, password);

        // Esperar hasta que se conecte o falle
        EventBits_t bits = xEventGroupWaitBits(wifi_event_group,
                                               WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                               pdFALSE,
                                               pdFALSE,
                                               pdMS_TO_TICKS(10000)); // Tiempo de espera para conexión (10s)

        if (bits & WIFI_CONNECTED_BIT) {
            ESP_LOGI(TAG_STA, "Conexion exitosa. Modo STA activado.");
            return; // Conectado exitosamente, permanecer en STA
        } else {
            ESP_LOGE(TAG_STA, "No se pudo conectar. Cambiando a modo AP...");
            ESP_ERROR_CHECK(esp_wifi_stop());
        }
    }

    // Si no hay credenciales o no se pudo conectar, activar modo AP
    start_softap();
}
