#include <stdio.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "cJSON.h"

#include "credentials.h"

static const char *TAG_MQTT = "MQTT_ThingsBoard";


esp_mqtt_client_handle_t cliente_mqtt;  // Cliente MQTT.


static void log_error_if_nonzero(const char *message, int error_code) {
    if (error_code != 0)
        ESP_LOGE(TAG_MQTT, "Last error %s: 0x%x", message, error_code);
}


/*
 * @brief Event handler registered to receive MQTT events
 *
 *  This function is called by the MQTT client event loop.
 *
 * @param handler_args user data registered to the event.
 * @param base Event base for the handler(always MQTT Base in this example).
 * @param event_id The id for the received event.
 * @param event_data The data for the event, esp_mqtt_event_handle_t.
 */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
//    ESP_LOGD(TAG_MQTT, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG_MQTT, "MQTT_EVENT_CONNECTED");
        msg_id = esp_mqtt_client_publish(client, "/topic/qos1", "data_3", 0, 1, 0);
        ESP_LOGI(TAG_MQTT, "sent publish successful, msg_id=%d", msg_id);

        msg_id = esp_mqtt_client_subscribe(client, "/topic/qos0", 0);
        ESP_LOGI(TAG_MQTT, "sent subscribe successful, msg_id=%d", msg_id);

        msg_id = esp_mqtt_client_subscribe(client, "/topic/qos1", 1);
        ESP_LOGI(TAG_MQTT, "sent subscribe successful, msg_id=%d", msg_id);

        msg_id = esp_mqtt_client_unsubscribe(client, "/topic/qos1");
        ESP_LOGI(TAG_MQTT, "sent unsubscribe successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG_MQTT, "MQTT_EVENT_DISCONNECTED");
        break;
    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG_MQTT, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        msg_id = esp_mqtt_client_publish(client, "/topic/qos0", "data", 0, 0, 0);
        ESP_LOGI(TAG_MQTT, "sent publish successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG_MQTT, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG_MQTT, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG_MQTT, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG_MQTT, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(TAG_MQTT, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
        }
        break;
    default:
        ESP_LOGI(TAG_MQTT, "Other event id:%d", event->event_id);
        break;
    }
}

static void mqtt_app_start(void) {
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = THINGSBOARD_SERVER,
        .broker.address.port = 1883,
        .credentials.username = THINGSBOARD_TOKEN,  
    };

    cliente_mqtt = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(cliente_mqtt, ESP_EVENT_ANY_ID, mqtt_event_handler, cliente_mqtt);
    esp_mqtt_client_start(cliente_mqtt);
}

void mqtt_mandar_credenciales_telegram(void) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "Bot_chat_id", BOT_CHAT_ID);
    cJSON_AddStringToObject(root, "Bot_url", BOT_URL);
    
    char *post_data = cJSON_PrintUnformatted(root);
    esp_mqtt_client_publish(cliente_mqtt, "v1/devices/me/attributes", post_data, 0, 1, 0);
    cJSON_Delete(root);
    free(post_data);
}

void mqtt_mandar_datos(float humedad, float turbidez, float temperatura, bool riegoActivo, int Nivel_Agua) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "Humedad", humedad);
    cJSON_AddNumberToObject(root, "Turbidez", turbidez);
    cJSON_AddNumberToObject(root, "Temperatura", temperatura);
    cJSON_AddBoolToObject(root, "Riego_Activo", riegoActivo);
    cJSON_AddBoolToObject(root, "Nivel_Agua", Nivel_Agua);
    
    char *post_data = cJSON_PrintUnformatted(root);
    esp_mqtt_client_publish(cliente_mqtt, "v1/devices/me/telemetry", post_data, 0, 1, 0);
    cJSON_Delete(root);
    // Free is intentional, it's client responsibility to free the result of cJSON_Print.
    free(post_data);
}



void iniciar_mqtt(void) {
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("MQTT_CLIENT", ESP_LOG_VERBOSE);
    esp_log_level_set("MQTT_EXAMPLE", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_BASE", ESP_LOG_VERBOSE);
    esp_log_level_set("esp-tls", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT", ESP_LOG_VERBOSE);
    esp_log_level_set("OUTBOX", ESP_LOG_VERBOSE);

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
  

    mqtt_app_start();
}
