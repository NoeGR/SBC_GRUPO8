#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>

// Define pines y dirección I2C del LCD
#define I2C_MASTER_SCL_IO 33
#define I2C_MASTER_SDA_IO 32
#define I2C_MASTER_FREQ_HZ 400000
#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_ADDR 0x27

// Funciones públicas del LCD
void lcd_init(void);
void lcd_move_to(uint8_t x, uint8_t y);
void lcd_putstr(const char *str);
void actualizar_lcd(float humedad, float temperatura, float turbidez);

#endif // LCD_H
