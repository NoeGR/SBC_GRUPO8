#include "ADS1115.h"
#include "esp_log.h"
#include "driver/i2c.h"
#include <stdio.h>

static const char *TAG = "ADS1115";

/** Inicializa el ADS1115 con la dirección y puerto I2C */
void ADS1115_init(ADS1115_t *ads, i2c_port_t port, uint8_t address) {
    ads->i2c_port = port;
    ads->dev_addr = address;
    ads->pga_mode = ADS1115_PGA_2P048; // Valor predeterminado
    ads->rate = ADS1115_RATE_128;      // Valor predeterminado
    ESP_LOGI(TAG, "ADS1115 initialized with address 0x%02X", address);
}

/** Prueba la conexión con el dispositivo I2C */
bool ADS1115_test_connection(ADS1115_t *ads) {
    uint8_t data[2];
    esp_err_t err = i2c_master_read_from_device(ads->i2c_port, ads->dev_addr, data, 2, 1000 / portTICK_PERIOD_MS);
    return err == ESP_OK;
}

/** Escribe en un registro del ADS1115 */
void ADS1115_write_register(ADS1115_t *ads, uint8_t reg, uint16_t value) {
    uint8_t data[3] = {reg, (uint8_t)(value >> 8), (uint8_t)(value & 0xFF)};
    i2c_master_write_to_device(ads->i2c_port, ads->dev_addr, data, sizeof(data), 1000 / portTICK_PERIOD_MS);
}

/** Lee un registro del ADS1115 */
uint16_t ADS1115_read_register(ADS1115_t *ads, uint8_t reg) {
    uint8_t data[2];
    i2c_master_write_read_device(ads->i2c_port, ads->dev_addr, &reg, 1, data, 2, 1000 / portTICK_PERIOD_MS);
    return (data[0] << 8) | data[1];
}

/** Configura el multiplexor */
void ADS1115_set_mux(ADS1115_t *ads, uint8_t mux) {
    uint16_t config = ADS1115_read_register(ads, ADS1115_RA_CONFIG);
    config &= ~(0x07 << ADS1115_CFG_MUX_BIT);
    config |= (mux << ADS1115_CFG_MUX_BIT);
    ADS1115_write_register(ads, ADS1115_RA_CONFIG, config);
}

/** Configura el PGA (ganancia) */
void ADS1115_set_gain(ADS1115_t *ads, uint8_t gain) {
    uint16_t config = ADS1115_read_register(ads, ADS1115_RA_CONFIG);
    config &= ~(0x07 << ADS1115_CFG_PGA_BIT);
    config |= (gain << ADS1115_CFG_PGA_BIT);
    ADS1115_write_register(ads, ADS1115_RA_CONFIG, config);
    ads->pga_mode = gain;
}

/** Configura el modo de operación */
void ADS1115_set_mode(ADS1115_t *ads, uint8_t mode) {
    uint16_t config = ADS1115_read_register(ads, ADS1115_RA_CONFIG);
    config &= ~(1 << ADS1115_CFG_MODE_BIT);
    config |= (mode << ADS1115_CFG_MODE_BIT);
    ADS1115_write_register(ads, ADS1115_RA_CONFIG, config);
}

/** Configura la tasa de muestreo */
void ADS1115_set_rate(ADS1115_t *ads, uint8_t rate) {
    uint16_t config = ADS1115_read_register(ads, ADS1115_RA_CONFIG);
    config &= ~(0x07 << ADS1115_CFG_DR_BIT);
    config |= (rate << ADS1115_CFG_DR_BIT);
    ADS1115_write_register(ads, ADS1115_RA_CONFIG, config);
    ads->rate = rate;
}

/** Inicia una nueva conversión */
void ADS1115_trigger_conversion(ADS1115_t *ads) {
    uint16_t config = ADS1115_read_register(ads, ADS1115_RA_CONFIG);
    config |= (1 << ADS1115_CFG_OS_BIT);
    ADS1115_write_register(ads, ADS1115_RA_CONFIG, config);
}

/** Lee el valor convertido */
int16_t ADS1115_get_conversion(ADS1115_t *ads) {
    return (int16_t)ADS1115_read_register(ads, ADS1115_RA_CONVERSION);
}

/** Convierte el valor del ADC a milivoltios */
float ADS1115_to_millivolts(ADS1115_t *ads, int16_t value) {
    float scale = 0.0f;
    switch (ads->pga_mode) {
        case ADS1115_PGA_6P144: scale = 6.144f / 32768.0f; break;
        case ADS1115_PGA_4P096: scale = 4.096f / 32768.0f; break;
        case ADS1115_PGA_2P048: scale = 2.048f / 32768.0f; break;
        case ADS1115_PGA_1P024: scale = 1.024f / 32768.0f; break;
        case ADS1115_PGA_0P512: scale = 0.512f / 32768.0f; break;
        case ADS1115_PGA_0P256: scale = 0.256f / 32768.0f; break;
        default: scale = 2.048f / 32768.0f; break;
    }
    return value * scale * 1000.0f;
}

/** Configura el modo del pin ALERT/RDY */
void ADS1115_set_conversion_ready_pin_mode(ADS1115_t *ads) {
    ADS1115_write_register(ads, ADS1115_RA_HI_THRESH, 0x8000);
    ADS1115_write_register(ads, ADS1115_RA_LO_THRESH, 0x0000);
    uint16_t config = ADS1115_read_register(ads, ADS1115_RA_CONFIG);
    config &= ~(1 << ADS1115_CFG_COMP_POL_BIT);  // Polaridad activa baja
    config &= ~(0x03 << ADS1115_CFG_COMP_QUE_BIT); // Habilitar ALERT/RDY
    ADS1115_write_register(ads, ADS1115_RA_CONFIG, config);
}
