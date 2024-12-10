#include "nvs.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG_NVS = "NVS";

#include "nvs_flash.h"
#include "nvs.h"

void guardar_float(nvs_handle_t nvs_handle, const char *clave, float valor) {
    uint32_t valor_int;
    memcpy(&valor_int, &valor, sizeof(valor));  // Convertir float a uint32_t
    esp_err_t err = nvs_set_u32(nvs_handle, clave, valor_int);  // Guardar como uint32_t
    if (err == ESP_OK) {
        nvs_commit(nvs_handle);  // Confirmar cambios
        printf("Guardado %s: %.2f\n", clave, valor);
    } else {
        printf("Error guardando %s: %s\n", clave, esp_err_to_name(err));
    }
}
float cargar_float(nvs_handle_t nvs_handle, const char *clave, float valor_por_defecto) {
    uint32_t valor_int;
    esp_err_t err = nvs_get_u32(nvs_handle, clave, &valor_int);  // Leer como uint32_t
    if (err == ESP_OK) {
        float valor;
        memcpy(&valor, &valor_int, sizeof(valor));  // Convertir uint32_t a float
        printf("Cargado %s: %.2f\n", clave, valor);
        return valor;
    } else {
        printf("Error cargando %s: %s. Usando valor por defecto: %.2f\n", clave, esp_err_to_name(err), valor_por_defecto);
        return valor_por_defecto;
    }
}


void inicializar_nvs() {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // Si NVS está corrupto o necesita ser actualizado, borra y vuelve a inicializar
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    printf("NVS inicializado correctamente.\n");
}

// Guardar el estado de riego en NVS (manual o automático)
void guardar_estado_riego(bool riegoManual) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &nvs_handle);
    if (err == ESP_OK) {
        err = nvs_set_u8(nvs_handle, "riego_manual", riegoManual ? 1 : 0);
        if (err == ESP_OK) {
            nvs_commit(nvs_handle);
            ESP_LOGI(TAG_NVS, "Estado de riego guardado: %s", riegoManual ? "Manual" : "Automático");
        } else {
            ESP_LOGE(TAG_NVS, "Error al guardar el estado de riego: %s", esp_err_to_name(err));
        }
        nvs_close(nvs_handle);
    } else {
        ESP_LOGE(TAG_NVS, "Error al abrir NVS: %s", esp_err_to_name(err));
    }
}

// Cargar el estado de riego desde NVS
bool cargar_estado_riego() {
    nvs_handle_t nvs_handle;
    uint8_t estado = 0; // Valor por defecto: Automático
    esp_err_t err = nvs_open("storage", NVS_READONLY, &nvs_handle);
    if (err == ESP_OK) {
        err = nvs_get_u8(nvs_handle, "riego_manual", &estado);
        if (err == ESP_OK) {
            ESP_LOGI(TAG_NVS, "Estado de riego cargado: %s", estado ? "Manual" : "Automático");
        } else {
            ESP_LOGW(TAG_NVS, "No se encontró el estado de riego, usando valor por defecto: Automático");
        }
        nvs_close(nvs_handle);
    } else {
        ESP_LOGE(TAG_NVS, "Error al abrir NVS: %s", esp_err_to_name(err));
    }
    return estado == 1;
}

void guardar_ultima_lectura(float humedad, float turbidez, float temperatura) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &nvs_handle);
    if (err == ESP_OK) {
        uint32_t humedad_int, turbidez_int, temperatura_int;
        memcpy(&humedad_int, &humedad, sizeof(float));
        memcpy(&turbidez_int, &turbidez, sizeof(float));
        memcpy(&temperatura_int, &temperatura, sizeof(float));

        nvs_set_u32(nvs_handle, "ultima_humedad", humedad_int);
        nvs_set_u32(nvs_handle, "ultima_turbidez", turbidez_int);
        nvs_set_u32(nvs_handle, "ultima_temp", temperatura_int);

        nvs_commit(nvs_handle);
        ESP_LOGI("NVS", "Últimas lecturas guardadas: Humedad=%.2f, Turbidez=%.2f, Temperatura=%.2f",
                 humedad, turbidez, temperatura);
        nvs_close(nvs_handle);
    } else {
        ESP_LOGE("NVS", "Error al abrir NVS para guardar lecturas: %s", esp_err_to_name(err));
    }
}

void cargar_ultima_lectura(float *humedad, float *turbidez, float *temperatura) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("storage", NVS_READONLY, &nvs_handle);
    if (err == ESP_OK) {
        uint32_t humedad_int, turbidez_int, temperatura_int;

        if (nvs_get_u32(nvs_handle, "ultima_humedad", &humedad_int) == ESP_OK) {
            memcpy(humedad, &humedad_int, sizeof(float));  // Convertir de uint32_t a float
        } else {
            *humedad = 0.0;  // Valor por defecto si no está guardado
        }

        if (nvs_get_u32(nvs_handle, "ultima_turbidez", &turbidez_int) == ESP_OK) {
            memcpy(turbidez, &turbidez_int, sizeof(float));  // Convertir de uint32_t a float
        } else {
            *turbidez = 0.0;  // Valor por defecto si no está guardado
        }

        if (nvs_get_u32(nvs_handle, "ultima_temp", &temperatura_int) == ESP_OK) {
            memcpy(temperatura, &temperatura_int, sizeof(float));  // Convertir de uint32_t a float
        } else {
            *temperatura = 0.0;  // Valor por defecto si no está guardado
        }

        ESP_LOGI("NVS", "Últimas lecturas cargadas: Humedad=%.2f, Turbidez=%.2f, Temperatura=%.2f",
                 *humedad, *turbidez, *temperatura);
        nvs_close(nvs_handle);
    } else {
        ESP_LOGW("NVS", "Error al abrir NVS para cargar lecturas: %s", esp_err_to_name(err));
        *humedad = 0.0;
        *turbidez = 0.0;
        *temperatura = 0.0;  // Valores por defecto si no hay datos en NVS
    }
}


void guardar_umbral_humedad(float humedad) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &handle);
    if (err == ESP_OK) {
        guardar_float(handle, "umbral_humedad", humedad);
        nvs_close(handle);
    } else {
        printf("Error abriendo NVS para guardar: %s\n", esp_err_to_name(err));
    }
}

float cargar_umbral_humedad() {
    nvs_handle_t handle;
    float humedad_por_defecto = 65.0;  
    esp_err_t err = nvs_open("storage", NVS_READONLY, &handle);
    if (err == ESP_OK) {
        float humedad = cargar_float(handle, "umbral_humedad", humedad_por_defecto);
        nvs_close(handle);
        return humedad;
    } else {
        printf("Error abriendo NVS para cargar: %s. Usando valor por defecto.\n", esp_err_to_name(err));
        return humedad_por_defecto;
    }
}



