/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Likus - UPI-1 handheld console firmware
 */

/*
Шаблон программы для УПИ-1
Ликус 2026
*/

#include "engine.h"
#include "resources.h"

int main(void)
{
  // Setup
  UPI_START();
  bool wasPres = false;

  // Loop
  while (1)
  {
    draw_clear();
    wasPres = false;
    if(UPI_BTN_UP_PRESSED())
    {
      if(!wasPres)
      {
        UPI_SOUND_PLAY(10, 50);
        wasPres = true;
      }
      draw_bitmap(32, 10, 8, 8, ICON_BTN_UP_PRESSED, 0);
    }
    else
    {
      draw_bitmap(32, 12, 8, 8, ICON_BTN_UP, 0);
    }
    if(UPI_BTN_LF_PRESSED())
    {
      if(!wasPres)
      {
        UPI_SOUND_PLAY(50, 50);
        wasPres = true;
      }
      draw_bitmap(14, 28, 8, 8, ICON_BTN_LEFT_PRESSED, 0);
    }
    else
    {
      draw_bitmap(16, 28, 8, 8, ICON_BTN_LEFT, 0);
    }
    if(UPI_BTN_DW_PRESSED())
    {
      if(!wasPres)
      {
        UPI_SOUND_PLAY(100, 50);
        wasPres = true;
      }
      draw_bitmap(32, 44, 8, 8, ICON_BTN_DOWN_PRESSED, 0);
    }
    else
    {
      draw_bitmap(32, 42, 8, 8, ICON_BTN_DOWN, 0);
    }
    if(UPI_BTN_RG_PRESSED())
    {
      if(!wasPres)
      {
        UPI_SOUND_PLAY(150, 50);
        wasPres = true;
      }
      draw_bitmap(50, 28, 8, 8, ICON_BTN_RIGHT_PRESSED, 0);
    }
    else
    {
      draw_bitmap(48, 28, 8, 8, ICON_BTN_RIGHT, 0);
    }
    if(UPI_BTN_ACT1_PRESSED())
    {
      if(!wasPres)
      {
        UPI_SOUND_PLAY(200, 50);
        wasPres = true;
      }
      draw_bitmap(92, 32, 16, 16, ICON_BTN_ACT1_PRESSED, 0);
    }
    else
    {
      draw_bitmap(96, 36, 8, 8, ICON_BTN_ACT1, 0);
    }
    if(UPI_BTN_ACT2_PRESSED())
    {
      if(!wasPres)
      {
        UPI_SOUND_PLAY(250, 50);
        wasPres = true;
      }
      draw_bitmap(100, 16, 16, 16, ICON_BTN_ACT2_PRESSED, 0);
    }
    else
    {
      draw_bitmap(104, 20, 8, 8, ICON_BTN_ACT2, 0);
    }

    draw_update();
  }
}