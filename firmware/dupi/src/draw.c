/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Likus - UPI-1 handheld console firmware
 */

#include "draw.h"
#include <string.h>

uint8_t draw_buffer[OLED_BUF_SIZE];
static bool dirty = true;

// Перевод пиксельных координат в индекс буфера
static inline uint16_t _bufIdx(int16_t x, int16_t y)
{
    return (uint16_t)(y >> 3) * 128 + x;
}

void draw_init(void)
{
    draw_clear();
    draw_update();
}

void draw_clear(void)
{
    memset(draw_buffer, 0x00, OLED_BUF_SIZE);
    dirty = true;
}

void SetDirty()
{
    dirty = true;
}

void draw_fill(uint8_t value)
{
    memset(draw_buffer, value, OLED_BUF_SIZE);
    dirty = true;
}

void draw_setPixel(int16_t x, int16_t y, bool color)
{
    if (x < 0 || x >= 128 || y < 0 || y >= 64)
        return;
    uint16_t idx = _bufIdx(x, y);
    uint8_t bit = y & 0x07;
    if (color)
        draw_buffer[idx] |= (1 << bit);
    else
        draw_buffer[idx] &= ~(1 << bit);
    dirty = true;
}

bool draw_getPixel(int16_t x, int16_t y)
{
    if (x < 0 || x >= 128 || y < 0 || y >= 64)
        return false;
    return (draw_buffer[_bufIdx(x, y)] >> (y & 0x07)) & 1;
}

void draw_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, bool color)
{
    if (x0 == x1)
    {
        if (y0 > y1)
        {
            int16_t t = y0;
            y0 = y1;
            y1 = t;
        }
        for (int16_t y = y0; y <= y1; y++)
            draw_setPixel(x0, y, color);
    }
    else if (y0 == y1)
    {
        if (x0 > x1)
        {
            int16_t t = x0;
            x0 = x1;
            x1 = t;
        }
        for (int16_t x = x0; x <= x1; x++)
            draw_setPixel(x, y0, color);
    }
}

void draw_rect(int16_t x, int16_t y, uint8_t w, uint8_t h, bool color, bool fill)
{
    if (fill)
    {
        for (int16_t py = y; py < y + h; py++)
            for (int16_t px = x; px < x + w; px++)
                draw_setPixel(px, py, color);
    }
    else
    {
        draw_line(x, y, x + w - 1, y, color);
        draw_line(x, y + h - 1, x + w - 1, y + h - 1, color);
        draw_line(x, y, x, y + h - 1, color);
        draw_line(x + w - 1, y, x + w - 1, y + h - 1, color);
    }
}

void draw_bitmap(int16_t x, int16_t y, uint8_t w, uint8_t h, const uint8_t *data, bool transparent)
{
    int16_t img_bottom = y + h - 1;

    for (uint8_t page = 0; page < 8; page++)
    {
        uint8_t page_top = page * 8;
        uint8_t page_bottom = page_top + 7;
        if (page_bottom < y || page_top > img_bottom)
            continue;

        for (uint8_t col = 0; col < w; col++)
        {
            int16_t tx = x + col;
            if (tx < 0 || tx >= 128)
                continue;

            uint8_t out = 0;
            for (uint8_t bit = 0; bit < 8; bit++)
            {
                int16_t sy = page_top + bit;
                if (sy < y || sy > img_bottom)
                    continue;

                uint8_t rel_y = sy - y;
                uint8_t img_page = rel_y >> 3;
                uint8_t img_bit = rel_y & 0x07;
                uint16_t didx = (uint16_t)img_page * w + col;

                if ((data[didx] >> img_bit) & 0x01)
                {
                    out |= (1 << bit);
                }
            }

            uint16_t bidx = (uint16_t)page * 128 + tx;
            if (transparent)
                draw_buffer[bidx] |= out;
            else
                draw_buffer[bidx] = out;
        }
    }
    dirty = true;
}

// Единственная точка контакта с oled.c
void draw_update(void)
{
    if (!dirty)
        return;
    OLED_render(draw_buffer);
    dirty = false;
}

void draw_vspan(int16_t x, int16_t y0, int16_t y1, bool color)
{
    if (x < 0 || x >= 128)
        return;
    if (y0 < 0)
        y0 = 0;
    if (y1 >= 64)
        y1 = 63;
    if (y0 > y1)
        return;

    uint8_t start_page = y0 >> 3;
    uint8_t end_page = y1 >> 3;

    for (uint8_t page = start_page; page <= end_page; page++)
    {
        uint16_t idx = (uint16_t)page * 128 + x;
        uint8_t mask = 0;
        uint8_t py_base = page << 3;

        for (uint8_t bit = 0; bit < 8; bit++)
        {
            if ((py_base + bit) >= y0 && (py_base + bit) <= y1)
                mask |= (1 << bit);
        }
        if (color)
            draw_buffer[idx] |= mask;
        else
            draw_buffer[idx] &= ~mask;
    }
    dirty = true;
}

void draw_vspan_skip(int16_t x, int16_t y0, int16_t y1, bool color, uint8_t skip)
{
    if (x < 0 || x >= 128 || y0 > y1)
        return;
    if (y0 < 0)
        y0 = 0;
    if (y1 >= 64)
        y1 = 63;

    for (int16_t y = y0; y <= y1; y++)
    {
        uint8_t threshold = BAYER4[x & 3][y & 3];
        if (skip > threshold)
        {
            uint16_t idx = (uint16_t)(y >> 3) * 128 + x;
            uint8_t bit = y & 0x07;
            if (color)
                draw_buffer[idx] |= (1 << bit);
            else
                draw_buffer[idx] &= ~(1 << bit);
        }
    }
    dirty = true;
}