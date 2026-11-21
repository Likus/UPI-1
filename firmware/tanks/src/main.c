/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Likus - UPI-1 handheld console firmware
 */

// Танчики — бой на разрушаемой карте для консоли УПИ-1

#include "engine.h"
#include "resources.h"

#define CURRENT_TIME STK->CNT

#define MAP_SIZE_X 24
#define MAP_SIZE_Y 16
#define BLOCK_SIZE 4
#define FULL_BLOCK_SIZE 8
#define MAP_OFFSET 0

#define MAX_POS_X (MAP_SIZE_X - 1) * BLOCK_SIZE
#define MAX_POS_Y (MAP_SIZE_Y - 1) * BLOCK_SIZE

#define DIR_UP 0
#define DIR_RIGHT 1
#define DIR_DOWN 2
#define DIR_LEFT 3

#define PLAYER_MOVE_SPEED 2
#define ENEMY_MOVE_SPEED 1
#define ENEMY_MAX_COUNT 5
#define ENEMY_CHANGE_DIR_TIME 5000
#define ENEMY_ON_LVL 20
#define ENEMY_SPAWN_DELAY 5000

#define BULLET_OFFSET_FORWARD 2
#define BULLET_OFFSET_SIDE 3
#define BULLET_SPEED 3

enum GAME_STATE
{
  main_menu,
  game_paused,
  game,
  to_next_lvl,
  to_prev_lvl,
  end
};

const uint8_t ENEMY_SPAWN_X[] = {0, 48, 88};

enum GAME_STATE game_state;
enum GAME_STATE prev_state;
uint8_t player_pos_x;
uint8_t player_pos_y;
uint8_t player_dir;
uint8_t current_map[MAP_SIZE_X * MAP_SIZE_Y];
uint8_t current_lvl;
uint32_t checkInputGameDelay;
uint32_t lastTime;
uint8_t deltaTime;

uint8_t enemyToSpawn;
uint32_t enemySpawnDelay;
uint8_t enemy_count;
uint8_t enemy_active[ENEMY_MAX_COUNT];
uint8_t enemy_pos_x[ENEMY_MAX_COUNT];
uint8_t enemy_pos_y[ENEMY_MAX_COUNT];
uint8_t enemy_direct[ENEMY_MAX_COUNT];
uint32_t enemy_move_delay[ENEMY_MAX_COUNT];

bool bullet_active[ENEMY_MAX_COUNT + 1];
uint8_t bullet_pos_x[ENEMY_MAX_COUNT + 1];
uint8_t bullet_pos_y[ENEMY_MAX_COUNT + 1];
uint8_t bullet_dir[ENEMY_MAX_COUNT + 1];

uint32_t game_state_delay;

bool btn_act1_wasPressedThisFrame;
bool btn_act1_wasPressedPrevFrame;
bool btn_act2_wasPressedThisFrame;
bool btn_act2_wasPressedPrevFrame;

void DrawDigit(uint8_t digit, uint16_t posX, uint16_t posY);
void DrawNum(uint64_t num, uint16_t posX, uint16_t posY);
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
void EnemyMoveAndDraw(void);
void CheckEnemy(void);
void DrawPlayer(void);
void PlayerShoot(void);
void BulletsMoveAndDraw(void);
uint8_t CollisionWithWall(uint8_t posX, uint8_t posY, uint8_t dir);
void DrawHUD(void);

// ===================================================================================
// Main Function
// ===================================================================================
int main(void)
{
  UPI_START();

  game_state = main_menu;
  prev_state = end;

  deltaTime = 0;
  btn_act1_wasPressedThisFrame = false;
  btn_act2_wasPressedThisFrame = false;
  checkInputGameDelay = 0;
  lastTime = CURRENT_TIME;

  uint16_t end_game_melody_not = 0;
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

    if (btn_act2_wasPressedThisFrame && UPI_BTN_ACT2_PRESSED())
    {
      btn_act2_wasPressedPrevFrame = true;
      btn_act2_wasPressedThisFrame = false;
    }
    if (!btn_act2_wasPressedThisFrame && UPI_BTN_ACT2_PRESSED() && !btn_act2_wasPressedPrevFrame)
    {
      btn_act2_wasPressedThisFrame = true;
    }
    if (!UPI_BTN_ACT2_PRESSED())
    {
      btn_act2_wasPressedPrevFrame = false;
      btn_act2_wasPressedThisFrame = false;
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
      DrawHUD();
      CheckInputGame();
      EnemyMoveAndDraw();
      BulletsMoveAndDraw();
      DrawLevel();
      DrawPlayer();
      CheckEnemy();
      prev_state = game;
      if(btn_act2_wasPressedThisFrame)
      {
        game_state = game_paused;
      }
      break;
    case to_next_lvl:
      if (prev_state != to_next_lvl)
      {
        draw_clear();
        DrawLevel();
        DrawPlayer();
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
        DrawPlayer();
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
        /*if (current_lvl > 0)
        {
          current_lvl -= 1;
        }*/
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

      if (end_game_melody_not < melody_length)
      {
        UPI_SOUND_PLAY(melody[end_game_melody_not], 100);
        end_game_melody_not += 1;
      }

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

void DrawDigit(uint8_t digit, uint16_t posX, uint16_t posY)
{
  digit %= 10;
  uint8_t imgDigit[8];
  uint8_t b;
  for (b = 0; b < 8; b++)
  {
    imgDigit[b] = DIGITS[digit * 8 + b];
  }
  draw_bitmap(posX, posY, 8, 8, imgDigit, 1);
}

void DrawNum(uint64_t num, uint16_t posX, uint16_t posY)
{
  if (num == 0)
  {
    DrawDigit(num, posX, posY);
  }
  uint8_t pos = 0;
  while (num > 0)
  {
    DrawDigit(num, posX - (pos * 5), posY);
    num /= 10;
    pos++;
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
  uint8_t x, y;
  enemy_count = 0;
  enemyToSpawn = ENEMY_ON_LVL;
  memset(bullet_active, false, ENEMY_MAX_COUNT + 1);
  memset(enemy_move_delay, 0, ENEMY_MAX_COUNT);
  memset(enemy_active, false, ENEMY_MAX_COUNT);

  for (x = 0; x < MAP_SIZE_X; x++)
  {
    for (y = 0; y < MAP_SIZE_Y; y++)
    {
      uint16_t numBlock = (current_lvl * (MAP_SIZE_X / 2) * (MAP_SIZE_Y / 2)) + ((x / 2) + ((y / 2) * MAP_SIZE_X / 2));
      switch (Levels[numBlock])
      {
      case 0:
        current_map[x + y * MAP_SIZE_X] = 0;
        break;
      case 1:
        current_map[x + y * MAP_SIZE_X] = Levels[numBlock];
        break;
      case 2:
        current_map[x + y * MAP_SIZE_X] = Levels[numBlock];
        break;
      case 5:
        if (x % 2 == 1 || y % 2 == 1)
        {
          break;
        }
        current_map[x + y * MAP_SIZE_X] = 0;
        player_pos_x = x * BLOCK_SIZE;
        player_pos_y = y * BLOCK_SIZE;
        DrawPlayer();
        break;
      case 4:
        if (x % 2 == 1 || y % 2 == 1)
        {
          break;
        }
        current_map[x + y * MAP_SIZE_X] = 0;
        enemyToSpawn--;
        enemy_count += 1;
        enemy_active[enemy_count - 1] = true;
        enemy_pos_x[enemy_count - 1] = x * BLOCK_SIZE;
        enemy_pos_y[enemy_count - 1] = y * BLOCK_SIZE;
        enemy_direct[enemy_count - 1] = UPI_RANDOM() % 4;
        break;
      case 3:
        current_map[x + y * MAP_SIZE_X] = Levels[numBlock];
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

  for (y = 0; y < MAP_SIZE_Y; y += 2)
  {
    draw_bitmap(MAP_OFFSET + MAP_SIZE_X * BLOCK_SIZE, y * BLOCK_SIZE, FULL_BLOCK_SIZE, FULL_BLOCK_SIZE, BLOCK_WALL, 0);
  }

  for (x = 0; x < MAP_SIZE_X; x++)
  {
    for (y = 0; y < MAP_SIZE_Y; y++)
    {
      switch (current_map[x + y * MAP_SIZE_X])
      {
      case 0:
        break;
      case 1:
        if (x % 2 == 1 || y % 2 == 1)
        {
          break;
        }
        draw_bitmap(MAP_OFFSET + x * BLOCK_SIZE, y * BLOCK_SIZE, FULL_BLOCK_SIZE, FULL_BLOCK_SIZE, BLOCK_WALL, 0);
        break;
      case 2:
        draw_bitmap(MAP_OFFSET + x * BLOCK_SIZE, y * BLOCK_SIZE, BLOCK_SIZE, BLOCK_SIZE, BLOCK_WALL_DESTROYABLE, 1);
        break;
      case 3:
        if (x % 2 == 1 || y % 2 == 1)
        {
          break;
        }
        draw_bitmap(MAP_OFFSET + x * BLOCK_SIZE, y * BLOCK_SIZE, FULL_BLOCK_SIZE, FULL_BLOCK_SIZE, BLOCK_FLAG, 0);
        break;
      case 4:
        if (x % 2 == 1 || y % 2 == 1)
        {
          break;
        }
        draw_bitmap(MAP_OFFSET + x * BLOCK_SIZE, y * BLOCK_SIZE, FULL_BLOCK_SIZE, FULL_BLOCK_SIZE, BLOCK_ENEMY, 0);
        break;
      case 5:
        if (x % 2 == 1 || y % 2 == 1)
        {
          break;
        }
        draw_bitmap(MAP_OFFSET + x * BLOCK_SIZE, y * BLOCK_SIZE, FULL_BLOCK_SIZE, FULL_BLOCK_SIZE, BLOCK_PLAYER, 0);
        break;
      default:
        break;
      }
    }
  }
}

void DrawPause(void)
{
  draw_bitmap(44, 28, 8, 8, ICON_BTN_ACT2, 0);
  draw_bitmap(52, 28, 32, 8, LBL_PLAY, 0);
}

void CheckInputPause(void)
{
  if (btn_act2_wasPressedThisFrame)
  {
    game_state = game;
  }
}

void CheckInputGame(void)
{
  uint8_t dist = CollisionWithWall(player_pos_x, player_pos_y, player_dir);
  if (btn_act1_wasPressedThisFrame)
  {
    PlayerShoot();
  }
  if (UPI_BTN_UP_PRESSED())
  {
    if (player_dir != DIR_UP)
    {
      player_dir = DIR_UP;
    }
    else
    {
      if (player_pos_y > 0)
      {
        if (dist > PLAYER_MOVE_SPEED)
        {
          player_pos_y -= PLAYER_MOVE_SPEED;
          return;
        }
        else
        {
          player_pos_y -= dist;
          return;
        }
      }
    }
  }
  if (UPI_BTN_DW_PRESSED())
  {
    if (player_dir != DIR_DOWN)
    {
      player_dir = DIR_DOWN;
    }
    else
    {
      if (player_pos_y < MAX_POS_Y - BLOCK_SIZE)
      {
        if (dist > PLAYER_MOVE_SPEED)
        {
          player_pos_y += PLAYER_MOVE_SPEED;
          return;
        }
        else
        {
          player_pos_y += dist;
          return;
        }
      }
    }
  }
  if (UPI_BTN_LF_PRESSED())
  {
    if (player_dir != DIR_LEFT)
    {
      player_dir = DIR_LEFT;
    }
    else
    {
      if (player_pos_x > 0)
      {
        if (dist > PLAYER_MOVE_SPEED)
        {
          player_pos_x -= PLAYER_MOVE_SPEED;
          return;
        }
        else
        {
          player_pos_x -= dist;
          return;
        }
      }
    }
  }
  if (UPI_BTN_RG_PRESSED())
  {
    if (player_dir != DIR_RIGHT)
    {
      player_dir = DIR_RIGHT;
    }
    else
    {
      if (player_pos_x < MAX_POS_X - BLOCK_SIZE)
      {
        if (dist > PLAYER_MOVE_SPEED)
        {
          player_pos_x += PLAYER_MOVE_SPEED;
          return;
        }
        else
        {
          player_pos_x += dist;
          return;
        }
      }
    }
  }
}

void CheckPlayerPosition(uint8_t block)
{
  /*if (block == 5)
  {
    current_map[player_pos_x + (player_pos_y)*MAP_SIZE_Y] = 0;
    coins_on_lvl -= 1;
    UPI_SOUND_PLAY(192, 100);
    UPI_SOUND_PLAY(128, 100);
  }
  if (block == 2 && coins_on_lvl == 0)
  {
    game_state = to_next_lvl;
  }*/
}

void DrawEnemys()
{
  uint8_t c;
  for (c = 0; c < ENEMY_MAX_COUNT; c++)
  {
    if (!enemy_active[c])
    {
      continue;
    }
    uint8_t enemySprite[8];
    for (uint8_t b = 0; b < 8; b++)
    {
      enemySprite[b] = BLOCK_ENEMY[enemy_direct[c] * 8 + b];
    }
    draw_bitmap(MAP_OFFSET + enemy_pos_x[c], enemy_pos_y[c], FULL_BLOCK_SIZE, FULL_BLOCK_SIZE, enemySprite, 1);
  }
}

void EnemyMoveAndDraw()
{
  uint8_t c;
  if (enemy_count < ENEMY_MAX_COUNT && enemyToSpawn > 0)
  {
    enemySpawnDelay += deltaTime;
    if (enemySpawnDelay > ENEMY_SPAWN_DELAY)
    {
      enemySpawnDelay = 0;
      for (c = 0; c < ENEMY_MAX_COUNT; c++)
      {
        if (!enemy_active[c])
        {
          enemyToSpawn--;
          enemy_count++;
          enemy_active[c] = true;
          enemy_pos_y[c] = 0;
          enemy_pos_x[c] = ENEMY_SPAWN_X[UPI_RANDOM() % 3];
          break;
        }
      }
    }
  }
  for (c = 0; c < ENEMY_MAX_COUNT; c++)
  {
    if (!enemy_active[c])
    {
      continue;
    }
    uint8_t dist = CollisionWithWall(enemy_pos_x[c], enemy_pos_y[c], enemy_direct[c]);
    switch (enemy_direct[c])
    {
    case DIR_UP:
      if (enemy_pos_y[c] - 1 < 0)
      {
        enemy_direct[c] = UPI_RANDOM() % 4;
        break;
      }
      if (dist > 0)
      {
        enemy_pos_y[c] -= ENEMY_MOVE_SPEED;
      }
      else
      {
        enemy_direct[c] = UPI_RANDOM() % 4;
      }
      break;
    case DIR_DOWN:
      if (enemy_pos_y[c] + 1 > MAX_POS_Y - BLOCK_SIZE)
      {
        enemy_direct[c] = UPI_RANDOM() % 4;
        break;
      }
      if (dist > 0)
      {
        enemy_pos_y[c] += ENEMY_MOVE_SPEED;
      }
      else
      {
        enemy_direct[c] = UPI_RANDOM() % 4;
      }
      break;
    case DIR_LEFT:
      if (enemy_pos_x[c] - 1 < 0)
      {
        enemy_direct[c] = UPI_RANDOM() % 4;
        break;
      }
      if (dist > 0)
      {
        enemy_pos_x[c] -= ENEMY_MOVE_SPEED;
      }
      else
      {
        enemy_direct[c] = UPI_RANDOM() % 4;
      }
      break;
    case DIR_RIGHT:
      if (enemy_pos_x[c] + 1 > MAX_POS_X - BLOCK_SIZE)
      {
        enemy_direct[c] = UPI_RANDOM() % 4;
        break;
      }
      if (dist > 0)
      {
        enemy_pos_x[c] += ENEMY_MOVE_SPEED;
      }
      else
      {
        enemy_direct[c] = UPI_RANDOM() % 4;
      }
      break;
    default:
      break;
    }
    uint8_t enemySprite[8];
    for (uint8_t b = 0; b < 8; b++)
    {
      enemySprite[b] = BLOCK_ENEMY[enemy_direct[c] * 8 + b];
    }
    draw_bitmap(MAP_OFFSET + enemy_pos_x[c], enemy_pos_y[c], FULL_BLOCK_SIZE, FULL_BLOCK_SIZE, enemySprite, 1);

    enemy_move_delay[c] += deltaTime;
    if (enemy_move_delay[c] > ENEMY_CHANGE_DIR_TIME)
    {
      enemy_move_delay[c] -= ENEMY_CHANGE_DIR_TIME;
      if (UPI_RANDOM() % 2 == 0)
      {
        enemy_direct[c] = UPI_RANDOM() % 4;
      }
      else
      {
        if (bullet_active[c + 1])
          return;
        bullet_active[c + 1] = true;
        bullet_dir[c + 1] = enemy_direct[c];
        switch (enemy_direct[c])
        {
        case DIR_UP:
          bullet_pos_x[c + 1] = enemy_pos_x[c] + BULLET_OFFSET_SIDE;
          bullet_pos_y[c + 1] = enemy_pos_y[c] - BULLET_OFFSET_FORWARD;
          break;
        case DIR_RIGHT:
          bullet_pos_x[c + 1] = enemy_pos_x[c] + FULL_BLOCK_SIZE;
          bullet_pos_y[c + 1] = enemy_pos_y[c] + BULLET_OFFSET_SIDE;
          break;
        case DIR_DOWN:
          bullet_pos_x[c + 1] = enemy_pos_x[c] + BULLET_OFFSET_SIDE;
          bullet_pos_y[c + 1] = enemy_pos_y[c] + FULL_BLOCK_SIZE;
          break;
        case DIR_LEFT:
          bullet_pos_x[c + 1] = enemy_pos_x[c] - BULLET_OFFSET_FORWARD;
          bullet_pos_y[c + 1] = enemy_pos_y[c] + BULLET_OFFSET_SIDE;
          break;
        default:
          break;
        }
      }
    }
  }
}

void CheckEnemy()
{
  /*uint8_t c;
  for (c = 0; c < enemy_count; c++)
  {
    if (player_pos_x / BLOCK_SIZE == enemy_pos_x[c] && player_pos_y / BLOCK_SIZE == enemy_pos_y[c])
    {
      game_state = to_prev_lvl;
      return;
    }
  }*/
}

void DrawPlayer(void)
{
  uint8_t playerSprite[8];
  for (uint8_t b = 0; b < 8; b++)
  {
    playerSprite[b] = BLOCK_PLAYER[player_dir * 8 + b];
  }
  draw_bitmap(MAP_OFFSET + player_pos_x, player_pos_y, FULL_BLOCK_SIZE, FULL_BLOCK_SIZE, playerSprite, 1);
}

uint8_t CollisionWithWall(uint8_t posX, uint8_t posY, uint8_t dir)
{
  uint8_t targetX;
  uint8_t targetY;
  uint8_t secondTargetX;
  uint8_t secondTargetY;
  switch (dir)
  {
  case DIR_UP:
    targetX = posX;
    targetY = posY - BLOCK_SIZE;
    secondTargetX = posX + FULL_BLOCK_SIZE - 1;
    secondTargetY = targetY;
    break;
  case DIR_RIGHT:
    targetX = posX + FULL_BLOCK_SIZE + BLOCK_SIZE - 1;
    targetY = posY;
    secondTargetX = targetX;
    secondTargetY = posY + FULL_BLOCK_SIZE - 1;
    break;
  case DIR_DOWN:
    targetX = posX;
    targetY = posY + FULL_BLOCK_SIZE + BLOCK_SIZE - 1;
    secondTargetX = posX + FULL_BLOCK_SIZE - 1;
    secondTargetY = targetY;
    break;
  case DIR_LEFT:
    targetX = posX - BLOCK_SIZE;
    targetY = posY;
    secondTargetX = targetX;
    secondTargetY = targetY + FULL_BLOCK_SIZE - 1;
    break;
  default:
    return BLOCK_SIZE;
    break;
  }
  uint8_t tagetBlock = current_map[(targetX / BLOCK_SIZE) + ((targetY / BLOCK_SIZE) * MAP_SIZE_X)];
  uint8_t seconTargetBlock = current_map[(secondTargetX / BLOCK_SIZE) + ((secondTargetY / BLOCK_SIZE) * MAP_SIZE_X)];
  if (tagetBlock == 0 && seconTargetBlock == 0)
  {
    return BLOCK_SIZE;
  }
  else
  {
    switch (dir)
    {
    case DIR_UP:
      return posY - ((targetY / BLOCK_SIZE) + 1) * BLOCK_SIZE;
    case DIR_RIGHT:
      return ((targetX / BLOCK_SIZE) * BLOCK_SIZE) - FULL_BLOCK_SIZE - posX;
    case DIR_DOWN:
      return ((targetY / BLOCK_SIZE) * BLOCK_SIZE) - FULL_BLOCK_SIZE - posY;
    case DIR_LEFT:
      return posX - ((targetX / BLOCK_SIZE) + 1) * BLOCK_SIZE;
    default:
      return BLOCK_SIZE;
    }
  }
}

void PlayerShoot(void)
{
  if (bullet_active[0])
    return;
  bullet_active[0] = true;
  bullet_dir[0] = player_dir;
  UPI_SOUND_PLAY(32, 5);
  switch (player_dir)
  {
  case DIR_UP:
    bullet_pos_x[0] = player_pos_x + BULLET_OFFSET_SIDE;
    bullet_pos_y[0] = player_pos_y - BULLET_OFFSET_FORWARD;
    break;
  case DIR_RIGHT:
    bullet_pos_x[0] = player_pos_x + FULL_BLOCK_SIZE;
    bullet_pos_y[0] = player_pos_y + BULLET_OFFSET_SIDE;
    break;
  case DIR_DOWN:
    bullet_pos_x[0] = player_pos_x + BULLET_OFFSET_SIDE;
    bullet_pos_y[0] = player_pos_y + FULL_BLOCK_SIZE;
    break;
  case DIR_LEFT:
    bullet_pos_x[0] = player_pos_x - BULLET_OFFSET_FORWARD;
    bullet_pos_y[0] = player_pos_y + BULLET_OFFSET_SIDE;
    break;
  default:
    break;
  }
}

void BulletsMoveAndDraw(void)
{
  for (uint8_t n = 0; n < ENEMY_MAX_COUNT + 1; n++)
  {
    if (bullet_active[n])
    {

      uint8_t blockX1 = bullet_pos_x[n] / BLOCK_SIZE;
      uint8_t blockY1 = bullet_pos_y[n] / BLOCK_SIZE;
      uint8_t blockX2;
      uint8_t blockY2;

      switch (bullet_dir[n])
      {
      case DIR_UP:
      case DIR_DOWN:
        blockX2 = (bullet_pos_x[n] + 1) / BLOCK_SIZE;
        blockY2 = bullet_pos_y[n] / BLOCK_SIZE;
        break;
      case DIR_RIGHT:
      case DIR_LEFT:
        blockX2 = (bullet_pos_x[n]) / BLOCK_SIZE;
        blockY2 = (bullet_pos_y[n] + 1) / BLOCK_SIZE;
        break;
      default:
        break;
      }

      if (current_map[blockX1 + blockY1 * MAP_SIZE_X] == 2)
      {
        current_map[blockX1 + blockY1 * MAP_SIZE_X] = 0;
        bullet_active[n] = false;
        UPI_SOUND_PLAY(16, 5);
      }
      if (current_map[blockX2 + blockY2 * MAP_SIZE_X] == 2)
      {
        current_map[blockX2 + blockY2 * MAP_SIZE_X] = 0;
        bullet_active[n] = false;
        UPI_SOUND_PLAY(16, 5);
      }

      if (current_map[blockX1 + blockY1 * MAP_SIZE_X] == 3)
      {
        blockX1 = blockX1 / 2 * 2;
        blockY1 = blockY1 / 2 * 2;
        current_map[blockX1 + blockY1 * MAP_SIZE_X] = 0;
        current_map[blockX1 + 1 + blockY1 * MAP_SIZE_X] = 0;
        current_map[blockX1 + (blockY1 + 1) * MAP_SIZE_X] = 0;
        current_map[blockX1 + 1 + (blockY1 + 1) * MAP_SIZE_X] = 0;
        bullet_active[n] = false;
        game_state = to_prev_lvl;
        return;
      }
      if (current_map[blockX2 + blockY2 * MAP_SIZE_X] == 3)
      {
        blockX2 = blockX2 / 2 * 2;
        blockY2 = blockY2 / 2 * 2;
        current_map[blockX2 + blockY2 * MAP_SIZE_X] = 0;
        current_map[blockX2 + 1 + blockY2 * MAP_SIZE_X] = 0;
        current_map[blockX2 + (blockY2 + 1) * MAP_SIZE_X] = 0;
        current_map[blockX2 + 1 + (blockY2 + 1) * MAP_SIZE_X] = 0;
        bullet_active[n] = false;
        game_state = to_prev_lvl;
        return;
      }
      if (current_map[blockX1 + blockY1 * MAP_SIZE_X] != 0)
      {
        bullet_active[n] = false;
        UPI_SOUND_PLAY(64, 5);
      }
      if (current_map[blockX2 + blockY2 * MAP_SIZE_X] != 0)
      {
        bullet_active[n] = false;
        UPI_SOUND_PLAY(64, 5);
      }

      if (!bullet_active[n])
      {
        continue;
      }

      if (n > 0 &&
          bullet_pos_x[n] > player_pos_x - 2 &&
          bullet_pos_y[n] > player_pos_y - 2 &&
          bullet_pos_x[n] < player_pos_x + FULL_BLOCK_SIZE &&
          bullet_pos_y[n] < player_pos_y + FULL_BLOCK_SIZE)
      {
        game_state = to_prev_lvl;
        return;
      }
      if (n == 0)
      {
        for (uint8_t ne = 0; ne < ENEMY_MAX_COUNT; ne++)
        {
          if (enemy_active[ne])
          {
            if (bullet_pos_x[n] > enemy_pos_x[ne] - 2 &&
                bullet_pos_y[n] > enemy_pos_y[ne] - 2 &&
                bullet_pos_x[n] < enemy_pos_x[ne] + FULL_BLOCK_SIZE &&
                bullet_pos_y[n] < enemy_pos_y[ne] + FULL_BLOCK_SIZE)
            {
              bullet_active[n] = false;
              enemy_active[ne] = false;
              enemy_count--;
              UPI_SOUND_PLAY(32, 100);
              if (enemy_count == 0 && enemyToSpawn == 0)
              {
                game_state = to_next_lvl;
                return;
              }
              break;
            }
          }
        }
      }

      if (!bullet_active[n])
      {
        continue;
      }

      switch (bullet_dir[n])
      {
      case DIR_UP:
        if (bullet_pos_y[n] > BULLET_SPEED)
        {
          bullet_pos_y[n] -= BULLET_SPEED;
        }
        else
        {
          bullet_active[n] = false;
          continue;
        }
        break;
      case DIR_RIGHT:
        if (bullet_pos_x[n] < MAX_POS_X)
        {
          bullet_pos_x[n] += BULLET_SPEED;
        }
        else
        {
          bullet_active[n] = false;
          continue;
        }
        break;
      case DIR_DOWN:
        if (bullet_pos_y[n] < MAX_POS_Y)
        {
          bullet_pos_y[n] += BULLET_SPEED;
        }
        else
        {
          bullet_active[n] = false;
          continue;
        }
        break;
      case DIR_LEFT:
        if (bullet_pos_x[n] > BULLET_SPEED)
        {
          bullet_pos_x[n] -= BULLET_SPEED;
        }
        else
        {
          bullet_active[n] = false;
          continue;
        }
        break;
      default:
        break;
      }

      draw_bitmap(MAP_OFFSET + bullet_pos_x[n], bullet_pos_y[n], 2, 2, BLOCK_WALL, 1);
    }
  }
}

void DrawHUD(void)
{
  draw_bitmap(105, 0, FULL_BLOCK_SIZE, FULL_BLOCK_SIZE, BLOCK_ENEMY, 1);
  DrawNum(enemyToSpawn + enemy_count, 121, 0);

  draw_bitmap(112, 32, FULL_BLOCK_SIZE, FULL_BLOCK_SIZE, ICON_BTN_ACT2, 1);
  draw_bitmap(104, 40, 24, FULL_BLOCK_SIZE, LBL_PAUSE, 1);


  draw_bitmap(105, 56, FULL_BLOCK_SIZE, FULL_BLOCK_SIZE, BLOCK_FLAG, 1);
  DrawNum(current_lvl + 1, 121, 56);
}
