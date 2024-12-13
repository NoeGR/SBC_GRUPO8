#ifndef NVS_H
#define NVS_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Inicializar NVS
void inicializar_nvs(void);

// Guardar y cargar el umbral de humedad
void guardar_umbral_humedad(float umbral);
float cargar_umbral_humedad(void);

// Guardar y cargar el estado de riego (manual o automático)
void guardar_estado_riego(bool riegoManual);
bool cargar_estado_riego(void);

// Guardar y cargar las últimas lecturas de sensores
void guardar_ultima_lectura(float humedad, float turbidez, float temperatura);
void cargar_ultima_lectura(float *humedad, float *turbidez, float *temperatura);

void guardar_umbral_humedad(float umbral);
float cargar_umbral_humedad();

// Guardar y cargar el último Update ID (Telegram/ThingsBoard)
void guardar_ultimo_update_id(int update_id);
int cargar_ultimo_update_id(void);

// Guardar y cargar credenciales de Wi-Fi
void guardar_credenciales_wifi(const char *ssid, const char *password);
void cargar_credenciales_wifi(char *ssid, size_t ssid_len, char *password, size_t password_len);

#ifdef __cplusplus
}
#endif

#endif // NVS_H
