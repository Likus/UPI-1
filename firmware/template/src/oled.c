/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Likus - UPI-1 handheld console firmware
 */

// SSD1306 128x64 OLED driver
#include "oled.h"

static const uint8_t OLED_INIT_CMD[] = {
    OLED_DISPLAY_OFF,
    OLED_MULTIPLEX,   0x3F,                 // set multiplex ratio
    OLED_CHARGEPUMP,  0x14,                 // set DC-DC enable
    OLED_MEMORYMODE,  0x00,                 // set horizontal addressing mode
    OLED_COLUMNS,     0x00, 0x7F,           // set start and end column
    OLED_PAGES,       0x00, 0x07,           // set start and end page (0x00-0x07 = 8 pages)
    OLED_OFFSET,      0x00,                 // no offset
    OLED_COMPINS,     0x12,                 // set com pins hardware config
    OLED_XFLIP, OLED_YFLIP,                 // flip screen for correct orientation
    OLED_DISPLAY_ON                         // turn display on
};

void OLED_init(void) {
    I2C_init();
    I2C_start(OLED_ADDR);
    I2C_write(OLED_CMD_MODE);
    for (uint8_t i = 0; i < sizeof(OLED_INIT_CMD); i++) {
        I2C_write(OLED_INIT_CMD[i]);
    }
    I2C_stop();
}

// Отправка готового буфера на экран (постранично, 128 байт/страница)
void OLED_render(const uint8_t* buffer) {
    for (uint8_t page = 0; page < 8; page++) {
        // Команды: выбор страницы + сброс колонки в 0
        I2C_start(OLED_ADDR);
        I2C_write(OLED_CMD_MODE);
        I2C_write(OLED_PAGE | page);        // B0-B7: page address
        I2C_write(OLED_COLUMN_LOW | 0x00);  // нижние 4 бита колонки = 0
        I2C_write(OLED_COLUMN_HIGH | 0x00); // старшие 4 бита колонки = 0
        I2C_stop();

        // Данные: 128 байт текущей страницы
        I2C_start(OLED_ADDR);
        I2C_write(OLED_DAT_MODE);
        for (uint8_t col = 0; col < 128; col++) {
            I2C_write(buffer[page * 128 + col]);
        }
        I2C_stop();
    }
}

// Отправка одной команды (контраст, сон, инверсия)
void OLED_sendCommand(uint8_t cmd) {
    I2C_start(OLED_ADDR);
    I2C_write(OLED_CMD_MODE);
    I2C_write(cmd);
    I2C_stop();
}

// Отправка команды с аргументом (контраст: 0x81 + значение)
void OLED_sendCommand2(uint8_t cmd, uint8_t arg) {
    I2C_start(OLED_ADDR);
    I2C_write(OLED_CMD_MODE);
    I2C_write(cmd);
    I2C_write(arg);
    I2C_stop();
}