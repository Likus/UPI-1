/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Likus - UPI-1 handheld console firmware
 */

// Лабиринт — поиск выхода на 20 уровнях для консоли УПИ-1

#include "engine.h"
#include "resources.h"

#define CURRENT_TIME STK->CNT

#define MAP_SIZE 8
#define BLOCK_SIZE 8
#define MAP_OFFSET 32

#define PLAYER_MOVE_DELAY 100
#define ENEMY_MAX_COUNT 4
#define ENEMY_MOVE_DELAY 300

enum GAME_STATE
{
  main_menu,
  game_paused,
  game,
  to_next_lvl,
  to_prev_lvl,
  end
};
enum GAME_STATE game_state;
enum GAME_STATE prev_state;
uint8_t coins_on_lvl;
uint8_t player_pos_x;
uint8_t player_pos_y;
uint8_t current_map[MAP_SIZE * MAP_SIZE];
uint8_t current_lvl;
uint32_t checkInputGameDelay;
uint32_t lastTime;
uint8_t deltaTime;

uint8_t enemy_count;
uint8_t enemy_pos_x[ENEMY_MAX_COUNT];
uint8_t enemy_pos_y[ENEMY_MAX_COUNT];
uint8_t enemy_direct[ENEMY_MAX_COUNT];
uint32_t enemy_move_delay;

uint32_t game_state_delay;

bool btn_act1_wasPressedThisFrame;
bool btn_act1_wasPressedPrevFrame;

void DrawMainMenu(void);
void CheckInputMainMenu(void);
void StartNewGame(void);
void LoadLevel(void);
void DrawLevel(void);
void DrawPause(void);
void CheckInputPause(void);
void CheckInputGame(void);
void CheckPlayerPosition(uint8_t block);
void DrawEnemys(void);
void EnemyMove(void);
void CheckEnemy(void);

// ===================================================================================
// Main Function
// ===================================================================================
int main(void)
{
  // Setup
  UPI_START();

  game_state = main_menu;
  prev_state = end;

  deltaTime = 0;
  btn_act1_wasPressedThisFrame = false;
  checkInputGameDelay = 0;
  enemy_move_delay = 0;
  lastTime = CURRENT_TIME;

  uint16_t end_game_melody_not = 0;
  // Loop
  while (1)
  {
    deltaTime = CURRENT_TIME - lastTime;
    lastTime = CURRENT_TIME;

    if (btn_act1_wasPressedThisFrame && UPI_BTN_ACT1_PRESSED())
    {
      btn_act1_wasPressedPrevFrame = true;
      btn_act1_wasPressedThisFrame = false;
    }

    if (!btn_act1_wasPressedThisFrame && UPI_BTN_ACT1_PRESSED() && !btn_act1_wasPressedPrevFrame)
    {
      btn_act1_wasPressedThisFrame = true;
    }

    if (!UPI_BTN_ACT1_PRESSED())
    {
      btn_act1_wasPressedPrevFrame = false;
      btn_act1_wasPressedThisFrame = false;
    }

    switch (game_state)
    {
    case main_menu:
      if (prev_state != main_menu)
      {
        draw_clear();
        DrawMainMenu();
        prev_state = main_menu;
      }
      CheckInputMainMenu();
      break;
    case game_paused:
      if (prev_state != game_paused)
      {
        draw_clear();
        DrawLevel();
        DrawPause();
        prev_state = game_paused;
      }
      CheckInputPause();
      break;
    case game:
      draw_clear();

      CheckInputGame();
      EnemyMove();

      DrawLevel();
      DrawEnemys();
      draw_bitmap(MAP_OFFSET + player_pos_x * BLOCK_SIZE, player_pos_y * BLOCK_SIZE, BLOCK_SIZE, BLOCK_SIZE, BLOCK_PLAYER, 0);
      CheckEnemy();
      prev_state = game;
      break;
    case to_next_lvl:
      if (prev_state != to_next_lvl)
      {
        draw_clear();
        DrawLevel();
        draw_bitmap(MAP_OFFSET + player_pos_x * BLOCK_SIZE, player_pos_y * BLOCK_SIZE, BLOCK_SIZE, BLOCK_SIZE, BLOCK_PLAYER, 1);
        draw_update();
        game_state_delay = 0;
        prev_state = to_next_lvl;

        UPI_SOUND_PLAY(128, 100);
        UPI_SOUND_PLAY(32, 100);
      }
      if (game_state_delay < 50000)
      {
        game_state_delay += deltaTime;
        break;
      }
      current_lvl += 1;
      if (current_lvl >= count_levels)
      {
        game_state = end;
        break;
      }
      LoadLevel();
      game_state = game_paused;
      break;
    case to_prev_lvl:
      if (prev_state != to_prev_lvl)
      {
        draw_clear();
        DrawLevel();
        DrawEnemys();
        draw_bitmap(MAP_OFFSET + player_pos_x * BLOCK_SIZE, player_pos_y * BLOCK_SIZE, BLOCK_SIZE, BLOCK_SIZE, BLOCK_PLAYER, 1);
        draw_update();
        game_state_delay = 0;
        prev_state = to_prev_lvl;

        UPI_SOUND_PLAY(16, 255);
        DLY_ms(255);
        UPI_SOUND_PLAY(32, 255);
      }
      if (game_state_delay < 500000)
      {
        game_state_delay += deltaTime;
        break;
      }
      else
      {
        if (current_lvl > 0)
        {
          current_lvl -= 1;
        }
        LoadLevel();
        game_state = game_paused;
      }
      break;
    case end:
      if (prev_state != end)
      {
        end_game_melody_not = 0;
        draw_clear();
        draw_bitmap(0, 0, 128, 64, END_GAME, 0);
      }

      if(end_game_melody_not < melody_length)
      {
        UPI_SOUND_PLAY(melody[end_game_melody_not], 100);
        end_game_melody_not += 1;
      }

      /*UPI_SOUND_PLAY(melody[end_game_melody_not], 150);
      end_game_melody_not += 1;
      if (end_game_melody_not >= melody_length)
      {
        end_game_melody_not = 0;
      }*/

      if (btn_act1_wasPressedThisFrame)
      {
        game_state = main_menu;
      }
      prev_state = end;
      break;
    default:
      break;
    }
    draw_update();
  }
}

void DrawMainMenu(void)
{
  draw_bitmap(0, 0, 128, 64, MAIN_MENU_IMG, 0);
}

void CheckInputMainMenu(void)
{
  if (btn_act1_wasPressedThisFrame)
  {
    StartNewGame();
  }
}

void StartNewGame(void)
{
  current_lvl = 0;
  game_state = game_paused;
  LoadLevel();
}

void LoadLevel(void)
{
  if (current_lvl >= count_levels)
  {
    current_lvl = 0;
  }
  coins_on_lvl = 0;
  uint8_t x, y;
  enemy_count = 0;
  for (x = 0; x < MAP_SIZE; x++)
  {
    for (y = 0; y < MAP_SIZE; y++)
    {
      uint16_t numBlock = (current_lvl * MAP_SIZE * MAP_SIZE) + (x + y * MAP_SIZE);
      switch (Levels[numBlock])
      {
      case 0:
      case 1:
        current_map[x + y * MAP_SIZE] = Levels[numBlock];
        break;
      case 2:
        current_map[x + y * MAP_SIZE] = Levels[numBlock];
        break;
      case 3:
        current_map[x + y * MAP_SIZE] = 0;
        player_pos_x = x;
        player_pos_y = y;
        break;
      case 4:
        current_map[x + y * MAP_SIZE] = 0;

        enemy_count += 1;
        enemy_pos_x[enemy_count - 1] = x;
        enemy_pos_y[enemy_count - 1] = y;
        enemy_direct[enemy_count - 1] = UPI_RANDOM() % 4;
        break;
      case 5:
        current_map[x + y * MAP_SIZE] = 5;
        coins_on_lvl += 1;
        break;
      default:
        break;
      }
    }
  }
}

void DrawLevel(void)
{
  uint8_t x, y;

  for (y = 0; y < MAP_SIZE; y++)
  {
    draw_bitmap(MAP_OFFSET - BLOCK_SIZE, y * BLOCK_SIZE, BLOCK_SIZE, BLOCK_SIZE, BLOCK_WALL, 0);
    draw_bitmap(MAP_OFFSET + MAP_SIZE * BLOCK_SIZE, y * BLOCK_SIZE, BLOCK_SIZE, BLOCK_SIZE, BLOCK_WALL, 0);
  }

  for (x = 0; x < MAP_SIZE; x++)
  {
    for (y = 0; y < MAP_SIZE; y++)
    {
      switch (current_map[x + y * MAP_SIZE])
      {
      case 0:
        break;
      case 1:
        draw_bitmap(MAP_OFFSET + x * BLOCK_SIZE, y * BLOCK_SIZE, BLOCK_SIZE, BLOCK_SIZE, BLOCK_WALL, 0);
        break;
      case 2:
        if (coins_on_lvl == 0)
        {
          draw_bitmap(MAP_OFFSET + x * BLOCK_SIZE, y * BLOCK_SIZE, BLOCK_SIZE, BLOCK_SIZE, BLOCK_DOOR, 0);
        }
        break;
      case 3:
        draw_bitmap(MAP_OFFSET + x * BLOCK_SIZE, y * BLOCK_SIZE, BLOCK_SIZE, BLOCK_SIZE, BLOCK_PLAYER, 0);
        break;
      case 4:
        draw_bitmap(MAP_OFFSET + x * BLOCK_SIZE, y * BLOCK_SIZE, BLOCK_SIZE, BLOCK_SIZE, BLOCK_ENEMY, 0);
        break;
      case 5:
        draw_bitmap(MAP_OFFSET + x * BLOCK_SIZE, y * BLOCK_SIZE, BLOCK_SIZE, BLOCK_SIZE, BLOCK_COIN, 0);
        break;
      default:
        break;
      }
    }
  }
}

void DrawPause(void)
{
  draw_bitmap(44, 28, 8, 8, ICON_BTN_ACT1, 0);
  draw_bitmap(52, 28, 32, 8, LBL_PLAY, 0);
}

void CheckInputPause(void)
{
  if (btn_act1_wasPressedThisFrame)
  {
    game_state = game;
  }
}

void CheckInputGame(void)
{
  if (checkInputGameDelay < (PLAYER_MOVE_DELAY))
  {
    checkInputGameDelay += deltaTime;
    return;
  }
  uint8_t target_block;
  if (UPI_BTN_UP_PRESSED())
  {
    if (player_pos_y > 0)
    {
      target_block = current_map[player_pos_x + (player_pos_y - 1) * MAP_SIZE];
      if (target_block == 0 || target_block == 2 || target_block == 5)
      {
        player_pos_y -= 1;
        CheckPlayerPosition(target_block);
        checkInputGameDelay = 0;
        return;
      }
    }
  }
  if (UPI_BTN_DW_PRESSED())
  {
    if (player_pos_y < 7)
    {
      target_block = current_map[player_pos_x + (player_pos_y + 1) * MAP_SIZE];
      if (target_block == 0 || target_block == 2 || target_block == 5)
      {
        player_pos_y += 1;
        CheckPlayerPosition(target_block);
        checkInputGameDelay = 0;
        return;
      }
    }
  }
  if (UPI_BTN_LF_PRESSED())
  {
    if (player_pos_x > 0)
    {
      target_block = current_map[player_pos_x - 1 + (player_pos_y)*MAP_SIZE];
      if (target_block == 0 || target_block == 2 || target_block == 5)
      {
        player_pos_x -= 1;
        CheckPlayerPosition(target_block);
        checkInputGameDelay = 0;
        return;
      }
    }
  }
  if (UPI_BTN_RG_PRESSED())
  {
    if (player_pos_x < 7)
    {
      target_block = current_map[player_pos_x + 1 + (player_pos_y)*MAP_SIZE];
      if (target_block == 0 || target_block == 2 || target_block == 5)
      {
        player_pos_x += 1;
        CheckPlayerPosition(target_block);
        checkInputGameDelay = 0;
        return;
      }
    }
  }
}

void CheckPlayerPosition(uint8_t block)
{
  if (block == 5)
  {
    current_map[player_pos_x + (player_pos_y)*MAP_SIZE] = 0;
    coins_on_lvl -= 1;
    UPI_SOUND_PLAY(192, 100);
    UPI_SOUND_PLAY(128, 100);
  }
  if (block == 2 && coins_on_lvl == 0)
  {
    game_state = to_next_lvl;
  }
}

void DrawEnemys()
{
  uint8_t c;
  for (c = 0; c < enemy_count; c++)
  {
    draw_bitmap(MAP_OFFSET + enemy_pos_x[c] * BLOCK_SIZE, enemy_pos_y[c] * BLOCK_SIZE, BLOCK_SIZE, BLOCK_SIZE, BLOCK_ENEMY, 0);
  }
}

void EnemyMove()
{
  if (enemy_move_delay < ENEMY_MOVE_DELAY)
  {
    enemy_move_delay += deltaTime;
    return;
  }
  enemy_move_delay = 0;
  uint8_t c;
  uint8_t target_block;
  for (c = 0; c < enemy_count; c++)
  {
    switch (enemy_direct[c])
    {
    case 0:
      if (enemy_pos_y[c] - 1 < 0)
      {
        enemy_direct[c] = UPI_RANDOM() % 4;
        break;
      }
      target_block = current_map[enemy_pos_x[c] + (enemy_pos_y[c] - 1) * MAP_SIZE];
      if (target_block == 0)
      {
        enemy_pos_y[c] -= 1;
      }
      else
      {
        enemy_direct[c] = UPI_RANDOM() % 4;
      }
      break;
    case 1:
      if (enemy_pos_y[c] + 1 > MAP_SIZE - 1)
      {
        enemy_direct[c] = UPI_RANDOM() % 4;
        break;
      }
      target_block = current_map[enemy_pos_x[c] + (enemy_pos_y[c] + 1) * MAP_SIZE];
      if (target_block == 0)
      {
        enemy_pos_y[c] += 1;
      }
      else
      {
        enemy_direct[c] = UPI_RANDOM() % 4;
      }
      break;
    case 2:
      if (enemy_pos_x[c] - 1 < 0)
      {
        enemy_direct[c] = UPI_RANDOM() % 4;
        break;
      }
      target_block = current_map[enemy_pos_x[c] - 1 + (enemy_pos_y[c]) * MAP_SIZE];
      if (target_block == 0)
      {
        enemy_pos_x[c] -= 1;
      }
      else
      {
        enemy_direct[c] = UPI_RANDOM() % 4;
      }
      break;
    case 3:
      if (enemy_pos_x[c] + 1 > MAP_SIZE - 1)
      {
        enemy_direct[c] = UPI_RANDOM() % 4;
        break;
      }
      target_block = current_map[enemy_pos_x[c] + 1 + (enemy_pos_y[c]) * MAP_SIZE];
      if (target_block == 0)
      {
        enemy_pos_x[c] += 1;
      }
      else
      {
        enemy_direct[c] = UPI_RANDOM() % 4;
      }
      break;
    default:
      break;
    }
  }
}

void CheckEnemy()
{
  uint8_t c;
  for (c = 0; c < enemy_count; c++)
  {
    if (player_pos_x == enemy_pos_x[c] && player_pos_y == enemy_pos_y[c])
    {
      game_state = to_prev_lvl;
      return;
    }
  }
}
