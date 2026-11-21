/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Likus - UPI-1 handheld console firmware
 */

// Sever Race Mini — гонка с препятствиями и рекордом во FLASH для УПИ-1

#include "engine.h"
#include "resources.h"
#include "flash.h"

#define RECORD_OFFSET 0
#define RECORD_ADDR RECORD_OFFSET

#define CURRENT_TIME STK->CNT
#define OBSTACLE_MAX_COUNT 16
#define player_start_x 8
#define player_start_y 32
#define player_max_x 112
#define player_max_y 56
#define player_min_x 0
#define player_min_y 8
#define obstacle_min_y 8
#define obstacle_max_y 56
#define timeForFuel 5000
#define BG_PARTICLE_COUNT 6
#define OBSTACLE_SPAWN_DELAY_MAX 64
#define OBSTACLE_SPAWN_DELAY_MIN 24
#define OBSTACLE_SPAWN_DELAY_SCORE_CORELATION 200
#define FUEL_SPAWN_DELAY_MAX 5000
#define FUEL_SPAWN_DELAY_MIN 2000
#define FUEL_SPAWN_DELAY_SCORE_CORELATION 10

uint16_t LoadHighScore();
void SaveHighScore();
void ShowTitle();
void StartNewGame(uint8_t skin);
void DrawDigit(uint8_t digit, uint16_t posX, uint16_t posY);
void DrawNum(uint64_t num, uint16_t posX, uint16_t posY);
void CheckPlayerInput();
void MoveObstacles(uint8_t spd);
void DrawObstacles();
void SpawnObstacle();
void CheckObstacleCollision();
void GameOver();
void DrawBG();
void MoveBG(uint8_t spd);

/*
0 - main menu
1 - game
2 - end game
*/
uint8_t gameState;
uint8_t obstacle_x[OBSTACLE_MAX_COUNT];
uint8_t obstacle_y[OBSTACLE_MAX_COUNT];
bool obstacle_active[OBSTACLE_MAX_COUNT];
uint8_t obstacle_view[OBSTACLE_MAX_COUNT];
bool btn_act1_wasPressedThisFrame;
bool btn_act1_wasPressedPrevFrame;
uint8_t player_x;
uint8_t player_y;
uint16_t obstacle_spawn_delay;
uint16_t obstacle_Spawn_Timer;
uint32_t lastTime;
uint8_t deltaTime;
uint32_t score;
uint16_t fuelTimer;
uint8_t fuel;
uint8_t endTimer;
uint16_t highScore;
uint32_t fuelSpawnDelay;
uint32_t fuelSpawnTimer;
uint8_t fuelX;
uint8_t fuelY;
bool fuelBonus;
/*
0 - boat
1 - track
*/
uint8_t mode;
bool newHighScore;

uint8_t bgParticleX[BG_PARTICLE_COUNT];
uint8_t bgParticleY[BG_PARTICLE_COUNT];
uint8_t bg1[8];
uint8_t bg2[8];
uint8_t player[16];
uint8_t obstacleView1Top[8];
uint8_t obstacleView2Top[8];
uint8_t obstacleView3Top[8];
uint8_t obstacleView1Bot[8];
uint8_t obstacleView2Bot[8];
uint8_t obstacleView3Bot[8];

// ===================================================================================
// Main Function
// ===================================================================================
int main(void)
{
  // Setup
  UPI_START();
  btn_act1_wasPressedThisFrame = false;

  highScore = LoadHighScore();
  ShowTitle();

  deltaTime = 0;
  lastTime = CURRENT_TIME;

  for (uint8_t n = 0; n < 8; n++)
  {
    obstacleView1Top[n] = IMG_OBSTACLE[n];
    obstacleView1Bot[n] = IMG_OBSTACLE[n + 24];
    obstacleView2Top[n] = IMG_OBSTACLE[n + 8];
    obstacleView2Bot[n] = IMG_OBSTACLE[n + 32];
    obstacleView3Top[n] = IMG_OBSTACLE[n + 16];
    obstacleView3Bot[n] = IMG_OBSTACLE[n + 40];
  }

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

    switch (gameState)
    {
    case 0:
      if (mode == 0)
      {
        if (UPI_BTN_RG_PRESSED())
        {
          mode = 1;
          UPI_SOUND_PLAY(127, 15);
          ShowTitle();
        }
      }
      else
      {
        if (UPI_BTN_LF_PRESSED())
        {
          mode = 0;
          UPI_SOUND_PLAY(127, 15);
          ShowTitle();
        }
      }
      if (btn_act1_wasPressedThisFrame)
      {
        StartNewGame(0);
      }
      break;
    case 1:
      fuelTimer += deltaTime;
      if (fuelTimer > timeForFuel)
      {
        fuelTimer -= timeForFuel;
        if (fuel == 0)
        {
          GameOver();
          continue;
        }
        fuel--;
      }
      draw_clear();

      CheckPlayerInput();

      uint8_t spd = 2;
      if (UPI_BTN_ACT1_PRESSED())
      {
        spd += 1;
      }
      if (UPI_BTN_ACT2_PRESSED())
      {
        spd -= 1;
      }

      MoveObstacles(spd);

      obstacle_Spawn_Timer += spd;
      if (obstacle_Spawn_Timer > obstacle_spawn_delay)
      {
        obstacle_Spawn_Timer -= obstacle_spawn_delay;
        SpawnObstacle();
      }
      if (obstacle_spawn_delay > OBSTACLE_SPAWN_DELAY_MIN)
      {
        obstacle_spawn_delay = OBSTACLE_SPAWN_DELAY_MAX - (score / OBSTACLE_SPAWN_DELAY_SCORE_CORELATION);
      }
      CheckObstacleCollision();

      if (fuelSpawnDelay < FUEL_SPAWN_DELAY_MAX)
      {
        fuelSpawnDelay = FUEL_SPAWN_DELAY_MIN + (score / FUEL_SPAWN_DELAY_SCORE_CORELATION);
      }

      if (fuelBonus)
      {
        if (fuelX > player_x - 4 && fuelX < player_x + 12 &&
            fuelY > player_y - 8 && fuelY < player_y + 8)
        {
          UPI_SOUND_PLAY(50, 50);
          UPI_SOUND_PLAY(75, 50);
          UPI_SOUND_PLAY(100, 50);
          fuelBonus = false;
          fuel += 50;
          if(fuel > 100)
          {
            fuel = 100;
          }
        }

        draw_bitmap(fuelX, fuelY, 8, 8, IMG_FUEL, 1);
        if (fuelX < spd)
        {
          fuelBonus = false;
        }
        else
        {
          fuelX -= spd;
        }
      }
      else
      {
        fuelSpawnTimer += spd;
        if (fuelSpawnTimer > fuelSpawnDelay)
        {
          fuelSpawnTimer -= fuelSpawnDelay;
          fuelBonus = true;
          fuelX = 128;
          fuelY = obstacle_min_y + UPI_RANDOM() % (obstacle_max_y - obstacle_min_y);
        }
      }

      draw_bitmap(player_x, player_y, 16, 8, player, 1);
      score += spd;
      draw_bitmap(0, 0, 128, 8, HUD, 0);
      DrawNum(score / 10, 121, 0);
      DrawNum(fuel, 29, 0);
      DrawBG();
      MoveBG(spd);
      draw_update();
      UPI_SOUND_PLAY(200 - (spd * 50), 5);
      break;
    case 2:
      draw_clear();
      if (endTimer < 32)
      {
        endTimer++;
        if (endTimer == 1)
        {
          MoveObstacles(1);
          DrawBG();
          MoveBG(1);
        }
        else if (endTimer == 2)
        {
          MoveObstacles(1);
          DrawBG();
          MoveBG(1);
        }
        else if (endTimer == 4)
        {
          MoveObstacles(1);
          DrawBG();
          MoveBG(1);
        }
        else if (endTimer == 8)
        {
          MoveObstacles(1);
          DrawBG();
          MoveBG(1);
        }
        else if (endTimer == 16)
        {
          MoveObstacles(1);
          DrawBG();
          MoveBG(1);
        }
        else if (endTimer == 32)
        {
          MoveObstacles(1);
          DrawBG();
          MoveBG(1);
        }
        else
        {
          DrawObstacles();
          DrawBG();
        }
      }
      else
      {
        DrawObstacles();
        DrawBG();
      }
      draw_bitmap(player_x, player_y, 16, 8, player, 1);
      draw_bitmap(8, 4, 64, 8, LBL_FUEL_EMPTY, 0);
      draw_bitmap(8, 16, 40, 8, LBL_RESULT, 0);
      DrawNum(score / 10, 80, 16);
      if (newHighScore)
      {
        draw_bitmap(8, 28, 32, 8, LBL_NEW, 0);
        draw_bitmap(40, 28, 32, 8, LBL_HSCORE, 0);
      }
      else
      {
        draw_bitmap(8, 28, 32, 8, LBL_HSCORE, 0);
        DrawNum(highScore, 80, 28);
      }

      draw_bitmap(48, 48, 8, 8, ICON_BTN_ACT1, 0);
      draw_bitmap(56, 48, 24, 8, LBL_EXIT, 0);
      draw_update();
      if (btn_act1_wasPressedThisFrame)
      {
        ShowTitle();
        UPI_SOUND_PLAY(127, 15);
      }
      break;
    default:
      break;
    }
  }
}

uint16_t LoadHighScore()
{
  uint16_t val = FLASH_END_read(RECORD_OFFSET);
  // 0xFFFF — стёртое состояние, значит рекорд ещё не записан
  return (val == 0xFFFF) ? 0 : val;
}

void SaveHighScore()
{
  FLASH_unlock();

  FLASH_END_erase();

  FLASH_END_write(RECORD_ADDR, highScore);

  FLASH_lock();
}

void StartNewGame(uint8_t skin)
{
  obstacle_spawn_delay = OBSTACLE_SPAWN_DELAY_MAX;
  obstacle_Spawn_Timer = 0;

  fuelSpawnDelay = FUEL_SPAWN_DELAY_MIN;
  fuelSpawnTimer = 0;
  fuelBonus = false;

  player_x = player_start_x;
  player_y = player_start_y;
  fuelTimer = 0;
  fuel = 100;
  score = 0;
  gameState = 1;
  newHighScore = false;

  if (mode == 0)
  {
    for (uint8_t n = 0; n < 8; n++)
    {
      bg1[n] = BG[n];
      bg2[n] = BG[n + 8];
    }
    for (uint8_t n = 0; n < 16; n++)
    {
      player[n] = IMG_PLAYER[n];
    }
  }
  else
  {
    for (uint8_t n = 0; n < 8; n++)
    {
      bg1[n] = BG[n + 16];
      bg2[n] = BG[n + 24];
    }
    for (uint8_t n = 0; n < 16; n++)
    {
      player[n] = IMG_PLAYER[n + 16];
    }
  }

  uint8_t num;
  for (num = 0; num < OBSTACLE_MAX_COUNT; num++)
  {
    obstacle_active[num] = false;
  }
  for (num = 0; num < BG_PARTICLE_COUNT; num++)
  {
    bgParticleX[num] = UPI_RANDOM() % 128;
    bgParticleY[num] = obstacle_min_y + (UPI_RANDOM() % (obstacle_max_y - obstacle_min_y));
  }

  UPI_SOUND_PLAY(127, 250);
  UPI_SOUND_PLAY(200, 250);
  UPI_SOUND_PLAY(75, 250);
}

void ShowTitle()
{
  draw_clear();
  draw_bitmap(0, 0, 128, 64, TITLE_SCREEN, 0);
  draw_bitmap(74, 0, 32, 8, LBL_HSCORE, 0);
  DrawNum(highScore, 122, 0);

  if (mode == 0)
  {
    draw_bitmap(16, 56, 8, 8, ICON_BTN_ACT1, 0);
    draw_bitmap(25, 56, 32, 8, LBL_START, 0);
    draw_bitmap(78, 56, 8, 8, ICON_BTN_RIGHT, 0);
    draw_bitmap(87, 56, 32, 8, LBL_SELECT, 0);
  }
  else
  {
    draw_bitmap(16, 56, 8, 8, ICON_BTN_LEFT, 0);
    draw_bitmap(25, 56, 32, 8, LBL_SELECT, 0);
    draw_bitmap(78, 56, 8, 8, ICON_BTN_ACT1, 0);
    draw_bitmap(87, 56, 32, 8, LBL_START, 0);
  }

  draw_update();

  gameState = 0;
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
  if(num == 0)
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

void CheckPlayerInput()
{
  if (UPI_BTN_UP_PRESSED() && player_y > player_min_y)
  {
    player_y--;
  }
  if (UPI_BTN_DW_PRESSED() && player_y < player_max_y)
  {
    player_y++;
  }
  if (UPI_BTN_LF_PRESSED() && player_x > player_min_x)
  {
    player_x--;
  }
  if (UPI_BTN_RG_PRESSED() && player_x < player_max_x)
  {
    player_x++;
  }
}

void MoveObstacles(uint8_t spd)
{
  uint8_t numObstacle;
  for (numObstacle = 0; numObstacle < OBSTACLE_MAX_COUNT; numObstacle++)
  {
    if (obstacle_active[numObstacle])
    {
      if (obstacle_x[numObstacle] < spd)
      {
        obstacle_active[numObstacle] = false;
      }
      else
      {
        obstacle_x[numObstacle] -= spd;
        switch (obstacle_view[numObstacle])
        {
        case 0:
          draw_bitmap(obstacle_x[numObstacle], obstacle_y[numObstacle], 8, 8, obstacleView1Top, 0);
          draw_bitmap(obstacle_x[numObstacle], obstacle_y[numObstacle] + 8, 8, 8, obstacleView1Bot, 1);
          break;
        case 1:
          draw_bitmap(obstacle_x[numObstacle], obstacle_y[numObstacle], 8, 8, obstacleView2Top, 0);
          draw_bitmap(obstacle_x[numObstacle], obstacle_y[numObstacle] + 8, 8, 8, obstacleView2Bot, 1);
          break;
        case 2:
          draw_bitmap(obstacle_x[numObstacle], obstacle_y[numObstacle], 8, 8, obstacleView3Top, 0);
          draw_bitmap(obstacle_x[numObstacle], obstacle_y[numObstacle] + 8, 8, 8, obstacleView3Bot, 1);
          break;
        default:
          draw_bitmap(obstacle_x[numObstacle], obstacle_y[numObstacle], 8, 8, obstacleView1Top, 0);
          draw_bitmap(obstacle_x[numObstacle], obstacle_y[numObstacle] + 8, 8, 8, obstacleView1Bot, 1);
          break;
        }
      }
    }
  }
}

void DrawObstacles()
{
  uint8_t numObstacle;
  for (numObstacle = 0; numObstacle < OBSTACLE_MAX_COUNT; numObstacle++)
  {
    if (obstacle_active[numObstacle])
    {
      switch (obstacle_view[numObstacle])
      {
      case 0:
        draw_bitmap(obstacle_x[numObstacle], obstacle_y[numObstacle], 8, 8, obstacleView1Top, 0);
        draw_bitmap(obstacle_x[numObstacle], obstacle_y[numObstacle] + 8, 8, 8, obstacleView1Bot, 1);
        break;
      case 1:
        draw_bitmap(obstacle_x[numObstacle], obstacle_y[numObstacle], 8, 8, obstacleView2Top, 0);
        draw_bitmap(obstacle_x[numObstacle], obstacle_y[numObstacle] + 8, 8, 8, obstacleView2Bot, 1);
        break;
      case 2:
        draw_bitmap(obstacle_x[numObstacle], obstacle_y[numObstacle], 8, 8, obstacleView3Top, 0);
        draw_bitmap(obstacle_x[numObstacle], obstacle_y[numObstacle] + 8, 8, 8, obstacleView3Bot, 1);
        break;
      default:
        draw_bitmap(obstacle_x[numObstacle], obstacle_y[numObstacle], 8, 8, obstacleView1Top, 0);
        draw_bitmap(obstacle_x[numObstacle], obstacle_y[numObstacle] + 8, 8, 8, obstacleView1Bot, 1);
        break;
      }
    }
  }
}

void SpawnObstacle()
{
  for (uint8_t numObstacle = 0; numObstacle < OBSTACLE_MAX_COUNT; numObstacle++)
  {
    if (!obstacle_active[numObstacle])
    {
      obstacle_y[numObstacle] = obstacle_min_y + UPI_RANDOM() % (obstacle_max_y - obstacle_min_y);
      obstacle_x[numObstacle] = 128;
      obstacle_view[numObstacle] = UPI_RANDOM() % 3;
      obstacle_active[numObstacle] = true;
      return;
    }
  }
}

void CheckObstacleCollision()
{
  uint8_t obsNum;
  for (obsNum = 0; obsNum < OBSTACLE_MAX_COUNT; obsNum++)
  {
    if (obstacle_active[obsNum])
    {
      if (obstacle_x[obsNum] > player_x - 4 && obstacle_x[obsNum] < player_x + 12 &&
          obstacle_y[obsNum] > player_y - 14 && obstacle_y[obsNum] < player_y + 6)
      {
        if (fuel > 10)
        {
          fuel -= 10;
        }
        else
        {
          fuel = 0;
        }
        obstacle_active[obsNum] = false;
        UPI_SOUND_PLAY(2, 250);
        return;
      }
    }
  }
}

void GameOver()
{
  endTimer = 0;
  if (score / 10 > highScore)
  {
    newHighScore = true;
    highScore = score / 10;
    SaveHighScore(highScore);
  }
  gameState = 2;

  UPI_SOUND_PLAY(75, 200);
  UPI_SOUND_PLAY(75, 200);
  UPI_SOUND_PLAY(200, 200);
  UPI_SOUND_PLAY(200, 200);
  UPI_SOUND_PLAY(127, 200);
  UPI_SOUND_PLAY(127, 200);
}

void DrawBG()
{
  for (uint8_t num = 0; num < BG_PARTICLE_COUNT; num++)
  {
    if (num < BG_PARTICLE_COUNT / 2)
    {
      draw_bitmap(bgParticleX[num], bgParticleY[num], 8, 8, bg1, 1);
    }
    else
    {
      draw_bitmap(bgParticleX[num], bgParticleY[num], 8, 8, bg2, 1);
    }
  }
}

void MoveBG(uint8_t spd)
{
  for (uint8_t num = 0; num < BG_PARTICLE_COUNT; num++)
  {
    if (bgParticleX[num] < spd)
    {
      bgParticleX[num] = 128;
    }
    else
    {
      bgParticleX[num] -= spd;
    }
  }
}
