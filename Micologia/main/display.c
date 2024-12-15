#include <stdio.h>
#include <string.h>
#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "display.h"

#define LCD_CMD_CLEAR 0x01
#define LCD_CMD_HOME 0x02
#define LCD_CMD_ENTRY_MODE 0x04
#define LCD_CMD_DISPLAY_CONTROL 0x08
#define LCD_CMD_FUNCTION_SET 0x20
#define LCD_CMD_CGRAM 0x40
#define LCD_CMD_DDRAM 0x80

#define MASK_RS 0x01
#define MASK_RW 0x02
#define MASK_E 0x04
#define SHIFT_BACKLIGHT 3
#define SHIFT_DATA 4

bool printed=false;

static void i2c_master_init() {
    i2c_config_t config = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    i2c_param_config(I2C_MASTER_NUM, &config);
    i2c_driver_install(I2C_MASTER_NUM, I2C_MODE_MASTER, 0, 0, 0);
}

static void lcd_write_byte(uint8_t data) {
    uint8_t backlight = 1 << SHIFT_BACKLIGHT;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (I2C_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, data | backlight, true);
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
}

static void lcd_send_nibble(uint8_t nibble, bool is_command) {
    uint8_t data = ((nibble & 0x0F) << SHIFT_DATA);
    if (!is_command) {
        data |= MASK_RS;
    }
    lcd_write_byte(data | MASK_E);
    lcd_write_byte(data);
}

static void lcd_send_byte(uint8_t byte, bool is_command) {
    lcd_send_nibble(byte >> 4, is_command);
    lcd_send_nibble(byte, is_command);
    if (is_command && (byte == LCD_CMD_CLEAR || byte == LCD_CMD_HOME)) {
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

 void lcd_init() {
	i2c_master_init();
    vTaskDelay(pdMS_TO_TICKS(50));
    for (int i = 0; i < 3; i++) {
        lcd_send_nibble(0x03, true);
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    lcd_send_nibble(0x02, true);
    vTaskDelay(pdMS_TO_TICKS(2));
    lcd_send_byte(LCD_CMD_FUNCTION_SET | 0x08, true);
    lcd_send_byte(LCD_CMD_DISPLAY_CONTROL | 0x04, true);
    lcd_send_byte(LCD_CMD_CLEAR, true);
    vTaskDelay(pdMS_TO_TICKS(5));
    lcd_send_byte(LCD_CMD_ENTRY_MODE | 0x02, true);
}

 void lcd_move_to(uint8_t x, uint8_t y) {
    uint8_t address = x;
    if (y == 1) address += 0x40;
    lcd_send_byte(LCD_CMD_DDRAM | address, true);
}

 void lcd_putstr(const char *str) {
    while (*str) {
        lcd_send_byte(*str++, false);
    }
}
void actualizar_lcd(float humedad, float temperatura, float turbidez) {
    char linea1[16], linea2[16], linea3[16];
    snprintf(linea1, sizeof(linea1), "Humedad: %.0f%%", humedad);
    snprintf(linea2, sizeof(linea2), "Temperatura:%.0fC", temperatura);
    snprintf(linea3, sizeof(linea3), "Claridad:%.0f%%", turbidez);
    lcd_move_to(0, 0);  // Mueve a la primera línea
    lcd_putstr("                ");  
    lcd_move_to(0, 0);
    lcd_putstr(linea1);
if(!printed){
    lcd_move_to(0, 1);  
    lcd_putstr("                "); 
    lcd_move_to(0, 1);
    lcd_putstr(linea2);
    printed=true;
 }else{
    lcd_move_to(0, 1);  
    lcd_putstr("                ");  
    lcd_move_to(0, 1);
    lcd_putstr(linea3);
    printed=false;
    }
    
}