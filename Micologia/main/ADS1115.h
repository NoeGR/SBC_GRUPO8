#ifndef _ADS1115_H_
#define _ADS1115_H_

#include <stdint.h>
#include <stdbool.h>
#include "driver/i2c.h"

// Direcciones del ADS1115
#define ADS1115_ADDRESS_ADDR_GND    0x48 // Dirección si el pin ADDR está conectado a GND
#define ADS1115_ADDRESS_ADDR_VDD    0x49 // Dirección si el pin ADDR está conectado a VDD
#define ADS1115_ADDRESS_ADDR_SDA    0x4A // Dirección si el pin ADDR está conectado a SDA
#define ADS1115_ADDRESS_ADDR_SCL    0x4B // Dirección si el pin ADDR está conectado a SCL
#define ADS1115_DEFAULT_ADDRESS     ADS1115_ADDRESS_ADDR_GND

// Direcciones de registro
#define ADS1115_RA_CONVERSION       0x00
#define ADS1115_RA_CONFIG           0x01
#define ADS1115_RA_LO_THRESH        0x02
#define ADS1115_RA_HI_THRESH        0x03

// Bits y máscaras para el registro de configuración
#define ADS1115_CFG_OS_BIT          15
#define ADS1115_CFG_MUX_BIT         14
#define ADS1115_CFG_MUX_LENGTH      3
#define ADS1115_CFG_PGA_BIT         11
#define ADS1115_CFG_PGA_LENGTH      3
#define ADS1115_CFG_MODE_BIT        8
#define ADS1115_CFG_DR_BIT          7
#define ADS1115_CFG_DR_LENGTH       3
#define ADS1115_CFG_COMP_MODE_BIT   4
#define ADS1115_CFG_COMP_POL_BIT    3
#define ADS1115_CFG_COMP_LAT_BIT    2
#define ADS1115_CFG_COMP_QUE_BIT    1
#define ADS1115_CFG_COMP_QUE_LENGTH 2

// Multiplexores de entrada
#define ADS1115_MUX_P0_N1           0x00
#define ADS1115_MUX_P0_N3           0x01
#define ADS1115_MUX_P1_N3           0x02
#define ADS1115_MUX_P2_N3           0x03
#define ADS1115_MUX_P0_NG           0x04
#define ADS1115_MUX_P1_NG           0x05
#define ADS1115_MUX_P2_NG           0x06
#define ADS1115_MUX_P3_NG           0x07

// Ganancias del PGA
#define ADS1115_PGA_6P144           0x00
#define ADS1115_PGA_4P096           0x01
#define ADS1115_PGA_2P048           0x02
#define ADS1115_PGA_1P024           0x03
#define ADS1115_PGA_0P512           0x04
#define ADS1115_PGA_0P256           0x05

// Modos de operación
#define ADS1115_MODE_CONTINUOUS     0x00
#define ADS1115_MODE_SINGLESHOT     0x01

// Tasas de muestreo
#define ADS1115_RATE_8              0x00
#define ADS1115_RATE_16             0x01
#define ADS1115_RATE_32             0x02
#define ADS1115_RATE_64             0x03
#define ADS1115_RATE_128            0x04
#define ADS1115_RATE_250            0x05
#define ADS1115_RATE_475            0x06
#define ADS1115_RATE_860            0x07

// Modos del comparador
#define ADS1115_COMP_MODE_HYSTERESIS    0x00
#define ADS1115_COMP_MODE_WINDOW        0x01

// Polaridad del comparador
#define ADS1115_COMP_POL_ACTIVE_LOW     0x00
#define ADS1115_COMP_POL_ACTIVE_HIGH    0x01

// Opciones de cola del comparador
#define ADS1115_COMP_QUE_ASSERT1    0x00
#define ADS1115_COMP_QUE_ASSERT2    0x01
#define ADS1115_COMP_QUE_ASSERT4    0x02
#define ADS1115_COMP_QUE_DISABLE    0x03

typedef struct {
    i2c_port_t i2c_port;
    uint8_t dev_addr;
    uint8_t pga_mode;
    uint8_t rate;
} ADS1115_t;

void ADS1115_init(ADS1115_t *ads, i2c_port_t port, uint8_t address);
bool ADS1115_test_connection(ADS1115_t *ads);

void ADS1115_set_mux(ADS1115_t *ads, uint8_t mux);
void ADS1115_set_gain(ADS1115_t *ads, uint8_t gain);
void ADS1115_set_mode(ADS1115_t *ads, uint8_t mode);
void ADS1115_set_rate(ADS1115_t *ads, uint8_t rate);
void ADS1115_trigger_conversion(ADS1115_t *ads);
int16_t ADS1115_get_conversion(ADS1115_t *ads);

float ADS1115_to_millivolts(ADS1115_t *ads, int16_t value);

#endif /* _ADS1115_H_ */
