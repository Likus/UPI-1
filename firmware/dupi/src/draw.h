/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Likus - UPI-1 handheld console firmware
 */

#pragma once
#include "oled.h"
#include <stdint.h>
#include <stdbool.h>


#define OLED_BUF_SIZE (128 * 64 / 8)

static const uint8_t BAYER4[4][4] = {
    { 0,  8,  2, 10},
    {12,  4, 14,  6},
    { 3, 11,  1,  9},
    {15,  7, 13,  5}
};

// Глобальный буфер (если нужен прямой доступ извне)
extern uint8_t draw_buffer[OLED_BUF_SIZE];

// === Инициализация и очистка ===
void draw_init(void);
void draw_clear(void);
void draw_fill(uint8_t value);  // 0x00 = чёрный, 0xFF = белый
void SetDirty();

// === Примитивы ===
void draw_setPixel(int16_t x, int16_t y, bool color);
bool draw_getPixel(int16_t x, int16_t y);
void draw_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, bool color);
void draw_rect(int16_t x, int16_t y, uint8_t w, uint8_t h, bool color, bool fill);

// === Битмапы ===
/**
 * @param x,y - координаты левого верхнего угла (поддерживаются отрицательные)
 * @param w,h - размеры изображения в пикселях
 * @param data - массив в формате: вертикальный, LSB сверху, постраничный
 * @param transparent - true = наложение (нулевые биты не стирают фон)
 */
void draw_bitmap(int16_t x, int16_t y, uint8_t w, uint8_t h, const uint8_t* data, bool transparent);

// === Синхронизация ===
void draw_update(void);  // Вызывает OLED_render(draw_buffer)

void draw_vspan(int16_t x, int16_t y0, int16_t y1, bool color);
void draw_vspan_skip(int16_t x, int16_t y0, int16_t y1, bool color, uint8_t skip);
