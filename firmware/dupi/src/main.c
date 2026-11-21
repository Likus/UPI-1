/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Likus - UPI-1 handheld console firmware
 */

// DupI — raycaster-шутер от первого лица для консоли УПИ-1

#include "engine.h"
#include "resources.h"
#include "millis.h"
#include "fixedPoint.h"

#define FOV_SCALE 120
#define ROT_COS 254
#define ROT_SIN 22
#define MAPCELL(x, y) ((x) + (y) * LEVEL_SIZE)
#define MOVE_SPEED 20
#define ABS8(x) (((x) ^ ((x) >> 7)) - ((x) >> 7))

// Типы сущностей
#define ENTITY_NONE 0
#define ENTITY_MEDKIT 1
#define ENTITY_ENEMY 2
#define ENTITY_AMMO 3
#define ENTITY_KEY 4
#define ENTITY_EXIT 5

#define ENEMY_SPEED 8

#define PLAYER_START_AMMO 5
#define PLAYER_START_HEALTH 25
#define PLAYER_MAX_HEALTH 100

typedef struct
{
  fp_t x, y; // координаты в клетках карты (1.0 = 256)
  // fp_t dx, dy; // единичный вектор направления
  uint8_t angle;
} view_t;

typedef struct
{
  int16_t drawStartX;
  int16_t drawEndX;
  int16_t drawStartY;
  int16_t drawEndY;
  int16_t distInt; // Для проверки Z-буфера
  bool is_visible;
} sprite_proj_t;

typedef struct
{
  fp_t x, y;          // 4 байта
  uint8_t type;       // 1 байт (ENTITY_*)
  int8_t state;       // 1 байт
  uint8_t anim_frame;
  uint8_t anim_timer; // 1 байт
  uint8_t hurt_timer; // 1 байт
} entity_t;

typedef struct
{
  uint8_t x, y;
  uint8_t is_open;
  uint8_t anim_frame; // 0, 1, 2 (кадр анимации открытия)
} door_t;

static const int16_t DIR_COS[32] = {
    256, 251, 236, 213, 181, 142, 98, 50, 0, -50, -98, -142, -181, -213, -236, -251,
    -256, -251, -236, -213, -181, -142, -98, -50, 0, 50, 98, 142, 181, 213, 236, 251};

static const int16_t DIR_SIN[32] = {
    0, 50, 98, 142, 181, 213, 236, 251, 256, 251, 236, 213, 181, 142, 98, 50,
    0, -50, -98, -142, -181, -213, -236, -251, -256, -251, -236, -213, -181, -142, -98, -50};

static const int8_t WEAPON_BOB[16] = {
    0, 0, 1, 1, 2, 2, 1, 1, 0, 0, -1, -1, -2, -2, -1, -1};

void DrawDigit(uint8_t digit, uint16_t posX, uint16_t posY);
void DrawNum(uint64_t num, uint16_t posX, uint16_t posY);
void CheckInputAct();
void StateTitle();
void LoadGame();
void DrawGame();
void InputGame();
sprite_proj_t sprite_project(fp_t sx, fp_t sy, uint8_t base_size);
void sprite_draw(const sprite_proj_t *p, uint8_t tex_w, uint8_t tex_h, const uint8_t *data, const uint8_t *mask);
void DrawHud();
void UpdateEntities();
void RenderEntitys();
void player_shoot();
void render_damage_feedback();
void Dead();
void DrawDead();
void render_pickup_feedback();
int8_t check_door_in_front();
bool can_pass_through(int16_t mapX, int16_t mapY);
void StateWin();
bool has_line_of_sight(fp_t fromX, fp_t fromY, fp_t toX, fp_t toY);

enum GAME_STATE
{
  title,
  // game_paused,
  game,
  dead,
  gameover,
  win
  // to_next_lvl,
  // to_prev_lvl,
  // end
};

enum GAME_STATE currentState;
enum GAME_STATE prevState;
uint32_t lastTime;
uint32_t deltaTime;

uint32_t timer;
bool blinker;
uint16_t counter;
bool btn_act1_wasPressedThisFrame;
bool btn_act1_wasPressedPrevFrame;
bool btn_act2_wasPressedThisFrame;
bool btn_act2_wasPressedPrevFrame;
bool clearFlag;

uint16_t zBuffer[128];

view_t viewPlayer;
uint8_t playerHealth;
uint8_t playerAmmo;
uint8_t playerKeys;
uint8_t player_hurt_timer = 0;
uint8_t pickup_flash_timer = 0;

uint8_t weapon_bob_timer;
bool is_moving = false;
uint8_t shoot_cooldown = 0;
uint8_t flash_timer = 0;

// COUNT_ENTITYS × sizeof(entity_t) ≈ 340 байт RAM
#define COUNT_ENTITYS 34
entity_t entities[COUNT_ENTITYS];
int8_t center_enemy_id = -1;    // ID врага в центре экрана (-1 = никого)
fp_t center_enemy_dist = 32767; // для сортировки по близости

door_t doors[8];
uint8_t door_count = 0;
int8_t doorInFront = -1;

bool newRecord = false;

// ===main===
int main(void)
{
  UPI_START();
  MIL_init();

  currentState = title;
  prevState = title;
  timer = 0;
  blinker = false;
  counter = 0;
  btn_act1_wasPressedThisFrame = false;
  btn_act2_wasPressedThisFrame = false;
  clearFlag = true;

  while (1)
  {
    deltaTime = MIL_read() - lastTime;
    lastTime = MIL_read();

    CheckInputAct();

    if (clearFlag)
    {
      draw_clear();
    }
    clearFlag = true;

    switch (currentState)
    {
    case title:
      StateTitle();
      break;
    case game:
      InputGame();
      DrawGame();

      UpdateEntities();
      RenderEntitys();

      render_damage_feedback();

      DrawHud();
      render_pickup_feedback();
      break;
    case dead:
      DrawDead();
      break;
    case gameover:
      currentState = title;
      break;
    case win:
      StateWin();
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

void CheckInputAct()
{
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
}

void StateTitle()
{
  timer += deltaTime;
  if (timer > 750)
  {
    blinker = !blinker;
    timer -= 750;
  }

  uint8_t freq = doom_melody[counter][0];
  uint8_t ticks = doom_melody[counter][1];
  if (ticks > 0)
  {
    UPI_SOUND_PLAY(freq, ticks * 75);
  }
  counter++;
  if (doom_melody[counter][0] == 0 && doom_melody[counter][1] == 0)
  {
    counter = 0;
  }

  draw_bitmap(28, 8, 72, 48, IMG_TITLE, 0);
  if (blinker)
    draw_bitmap(60, 56, 8, 8, ICON_BTN_ACT1, 0);
  else
  {
    draw_bitmap(56, 52, 16, 16, ICON_BTN_ACT1_PRESSED, 0);
  }

  if (btn_act1_wasPressedThisFrame)
  {
    LoadGame();
    currentState = game;
  }
}

void LoadGame()
{
  viewPlayer.x = INT_TO_FP(1) + 128;
  viewPlayer.y = INT_TO_FP(1) + 128;
  viewPlayer.angle = 8;

  for (uint8_t i = 0; i < COUNT_ENTITYS; i++)
  {
    entities[i].type = ENTITY_NONE;
    entities[i].state = -1;
  }

  uint8_t numEntity = 0;

  door_count = 0;

  for (uint8_t x = 0; x < LEVEL_SIZE; x++)
  {
    for (uint8_t y = 0; y < LEVEL_SIZE; y++)
    {
      if (numEntity > COUNT_ENTITYS - 1)
      {
        numEntity = COUNT_ENTITYS - 1;
      }
      entities[numEntity].x = INT_TO_FP(x) + 128;
      entities[numEntity].y = INT_TO_FP(y) + 128;
      entities[numEntity].anim_frame = 0;
      entities[numEntity].anim_timer = 0;
      entities[numEntity].hurt_timer = 0;
      switch (Levels[x + y * LEVEL_SIZE])
      {
      case 2:
        entities[numEntity].type = ENTITY_ENEMY;
        entities[numEntity].state = 3;
        numEntity++;
        break;
      case 3:
        entities[numEntity].type = ENTITY_MEDKIT;
        entities[numEntity].state = 1;
        numEntity++;
        break;
      case 4:
        entities[numEntity].type = ENTITY_AMMO;
        entities[numEntity].state = 1;
        numEntity++;
        break;
      case 5:
        entities[numEntity].type = ENTITY_KEY;
        entities[numEntity].state = 1;
        numEntity++;
        break;
      case 6:
        viewPlayer.x = INT_TO_FP(x) + 128;
        viewPlayer.y = INT_TO_FP(y) + 128;
        break;
      case 7:
        entities[numEntity].type = ENTITY_EXIT;
        entities[numEntity].state = 1;
        numEntity++;
        break;
      case 8:
        doors[door_count].x = x;
        doors[door_count].y = y;
        doors[door_count].is_open = 0;
        door_count++;
        break;
      case 0:
      case 1:
      default:
        break;
      }
    }
  }

  pickup_flash_timer = 0;
  player_hurt_timer = 0;
  playerHealth = PLAYER_START_HEALTH;
  playerAmmo = PLAYER_START_AMMO;
  playerKeys = 0;
}

void DrawGame()
{
  fp_t planeX = FP_MUL(DIR_SIN[viewPlayer.angle], FOV_SCALE);
  fp_t planeY = FP_MUL(-DIR_COS[viewPlayer.angle], FOV_SCALE);

  for (int16_t x = 0; x < 128; x++)
  {
    fp_t cameraX = (x << 2) - 256;

    int16_t mapX = FP_TO_INT(viewPlayer.x);
    int16_t mapY = FP_TO_INT(viewPlayer.y);

    fp_t rayDirX = DIR_COS[viewPlayer.angle] + FP_MUL(planeX, cameraX);
    fp_t rayDirY = DIR_SIN[viewPlayer.angle] + FP_MUL(planeY, cameraX);

    int32_t deltaDistX = (rayDirX == 0) ? 32767 : FP_DIV(256, FP_ABS(rayDirX));
    int32_t deltaDistY = (rayDirY == 0) ? 32767 : FP_DIV(256, FP_ABS(rayDirY));

    int16_t stepX = (rayDirX < 0) ? -1 : 1;
    int16_t stepY = (rayDirY < 0) ? -1 : 1;

    int32_t sideDistX, sideDistY;

    if (rayDirX < 0)
    {
      sideDistX = FP_MUL(viewPlayer.x - INT_TO_FP(mapX), deltaDistX);
      if (sideDistX == 0)
        sideDistX = 1;
    }
    else
    {
      sideDistX = FP_MUL(INT_TO_FP(mapX + 1) - viewPlayer.x, deltaDistX);
      if (sideDistX == 0)
        sideDistX = 1;
    }

    if (rayDirY < 0)
    {
      sideDistY = FP_MUL(viewPlayer.y - INT_TO_FP(mapY), deltaDistY);
    }
    else
    {
      sideDistY = FP_MUL(INT_TO_FP(mapY + 1) - viewPlayer.y, deltaDistY);
    }

    int hit = 0;
    bool isHit = false;
    uint8_t isDoor = 0;
    while (1)
    {
      if (sideDistX < sideDistY)
      {
        sideDistX += deltaDistX;
        mapX += stepX;
        hit = 0;
      }
      else
      {
        sideDistY += deltaDistY;
        mapY += stepY;
        hit = 1;
      }
      if (mapX < 0 || mapX >= LEVEL_SIZE || mapY < 0 || mapY >= LEVEL_SIZE)
        break;

      if (Levels[MAPCELL(mapX, mapY)] == 1)
      {
        isHit = true;
        break; // стена найдена
      }
      bool findDoor = false;
      if (Levels[MAPCELL(mapX, mapY)] == 8)
      {
        for (uint8_t i = 0; i < door_count; i++)
        {
          if (doors[i].x == mapX && doors[i].y == mapY)
          {
            if (doors[i].is_open)
            {
              isDoor = 1;
            }
            else
            {
              isDoor = 2;
              isHit = true;
              findDoor = true;
            }
            break;
          }
        }
      }
      if (findDoor)
      {
        break;
      }
    }

    if (!isHit)
    {
      continue;
    }

    fp_t perpDist;
    if (hit == 0)
    {
      fp_t offset = (stepX == -1) ? 256 : 0; // 256 = FP_ONE = 1.0 в мире
      perpDist = FP_DIV(INT_TO_FP(mapX) - viewPlayer.x + offset, rayDirX);
    }
    else
    {
      fp_t offset = (stepY == -1) ? 256 : 0;
      perpDist = FP_DIV(INT_TO_FP(mapY) - viewPlayer.y + offset, rayDirY);
    }

    perpDist = FP_ABS(perpDist);
    if (perpDist < 32)
      perpDist = 32;
    if (perpDist > 2048)
      perpDist = 2048;

    int dist = FP_TO_INT(perpDist);

    if (x >= 0 && x < 128)
    {
      zBuffer[x] = dist;
    }

    int raw = 15 - (dist * 3);
    uint8_t density = (raw < 0) ? 0 : raw;

    int16_t height = FP_TO_INT(FP_DIV(32 * 256, perpDist));
    if (height > 64)
    {
      height = 64;
    }

    int16_t y0 = (64 - height) >> 1;
    int16_t y1 = y0 + height - 1;
    if (y1 > 55)
      y1 = 55;

    if (isDoor > 1)
    {
      for (int16_t y = y0; y <= y1; y++)
      {
        // Чередование: 1 белый, 1 чёрный пиксель
        if ((y & 1) == 0)
        {
          draw_setPixel(x, y, 1);
        }
      }
    }
    else
    {
      draw_vspan_skip(x, y0, y1, 1, density);
    }
  }
}

void InputGame()
{
  is_moving = false;
  if (UPI_BTN_UP_PRESSED() || UPI_BTN_DW_PRESSED())
  {
    is_moving = true;

    fp_t step = UPI_BTN_UP_PRESSED() ? MOVE_SPEED : -MOVE_SPEED;

    fp_t newX = viewPlayer.x + FP_MUL(DIR_COS[viewPlayer.angle], step);
    fp_t newY = viewPlayer.y + FP_MUL(DIR_SIN[viewPlayer.angle], step);

    int16_t mapX = FP_TO_INT(newX);
    int16_t mapY = FP_TO_INT(newY);

    if (mapX < 0)
      mapX = 0;
    if (mapX >= LEVEL_SIZE)
      mapX = LEVEL_SIZE - 1;
    if (mapY < 0)
      mapY = 0;
    if (mapY >= LEVEL_SIZE)
      mapY = LEVEL_SIZE - 1;

    // Проверка коллизии по X (стена проверяется по ТЕКУЩЕЙ Y игрока)
    int16_t curY = FP_TO_INT(viewPlayer.y);
    if (can_pass_through(mapX, curY))
    {
      viewPlayer.x = newX;
    }

    // Проверка коллизии по Y (стена проверяется по уже ОБНОВЛЁННОЙ X игрока)
    int16_t curX = FP_TO_INT(viewPlayer.x);
    if (can_pass_through(curX, mapY))
    {
      viewPlayer.y = newY;
    }
  }
  if (UPI_BTN_LF_PRESSED())
  {
    viewPlayer.angle = (viewPlayer.angle + 1) & 31;
  }

  if (UPI_BTN_RG_PRESSED())
  {
    viewPlayer.angle = (viewPlayer.angle - 1) & 31; // кольцо 32 направлений
  }

  if (btn_act1_wasPressedThisFrame)
  {
    player_shoot();
  }

  doorInFront = check_door_in_front();

  if (doorInFront > -1 && btn_act2_wasPressedThisFrame)
  {
    if (!doors[doorInFront].is_open)
    {
      if (playerKeys > 0)
      {
        UPI_SOUND_PLAY(190, 250);
        UPI_SOUND_PLAY(190, 250);
        UPI_SOUND_PLAY(180, 250);
        UPI_SOUND_PLAY(180, 250);
        UPI_SOUND_PLAY(200, 100);
        doors[doorInFront].is_open = 1; 
        pickup_flash_timer = 5;
        playerKeys--;
      }
    }
  }

  if (shoot_cooldown > 0)
  {
    shoot_cooldown--;
    if(shoot_cooldown > 13)
    {
      UPI_SOUND_PLAY(160, 50);
    }
    if(shoot_cooldown > 11)
    {
      UPI_SOUND_PLAY(200,50);
    }
    if(shoot_cooldown > 9)
    {
      UPI_SOUND_PLAY(240,50);
    }
  }
}

sprite_proj_t sprite_project(fp_t sx, fp_t sy, uint8_t base_size)
{
  sprite_proj_t p = {0}; // по умолчанию не виден

  fp_t dx = sx - viewPlayer.x;
  fp_t dy = sy - viewPlayer.y;
  fp_t dirX = DIR_COS[viewPlayer.angle];
  fp_t dirY = DIR_SIN[viewPlayer.angle];

  fp_t transY = FP_MUL(dx, dirX) + FP_MUL(dy, dirY);
  fp_t transX = FP_MUL(dx, dirY) - FP_MUL(dy, dirX);

  if (transY <= 0)
    return p; // Позади камеры
  if (transY < 128)
    transY = 128;

  int16_t screenOffset = (int16_t)(((int32_t)transX * (64 * 256)) / ((int32_t)FOV_SCALE * transY));
  int16_t spriteScreenX = screenOffset + 64;

  int16_t spriteHeight = (int16_t)(((int32_t)base_size * 256) / transY);
  int16_t spriteWidth = spriteHeight;

  if (spriteHeight > 128)
    spriteHeight = 128;
  if (spriteWidth > 128)
    spriteWidth = 128;
  if (spriteWidth < 1)
    spriteWidth = 1;

  p.drawStartX = spriteScreenX - (spriteWidth >> 1);
  p.drawEndX = p.drawStartX + spriteWidth - 1;
  p.drawStartY = 32 - (spriteHeight >> 1);
  p.drawEndY = p.drawStartY + spriteHeight - 1;

  // Клиппинг
  if (p.drawStartX < 0)
    p.drawStartX = 0;
  if (p.drawEndX > 127)
    p.drawEndX = 127;
  if (p.drawStartX > p.drawEndX)
    return p; // Полностью за краем

  if (p.drawEndY > 55)
  {
    p.drawEndY = 55;
  }

  if (p.drawStartY > 55)
  {
    p.is_visible = false;
  }

  p.distInt = FP_TO_INT(transY);
  p.is_visible = true;
  return p;
}

void sprite_draw(const sprite_proj_t *p, uint8_t tex_w, uint8_t tex_h, const uint8_t *data, const uint8_t *mask)
{
  if (!p->is_visible)
    return;

  for (int16_t stripe = p->drawStartX; stripe <= p->drawEndX; stripe++)
  {
    if (p->distInt < zBuffer[stripe] + 1)
    {

      uint8_t startPage = p->drawStartY >> 3;
      uint8_t endPage = p->drawEndY >> 3;

      int16_t sx_rel = stripe - p->drawStartX;

      for (uint8_t page = startPage; page <= endPage; page++)
      {
        if (page > 7)
          break;

        uint8_t mask_byte = 0;
        uint8_t data_byte = 0;
        uint8_t y_base = page << 3;

        for (uint8_t bit = 0; bit < 8; bit++)
        {
          int16_t y = y_base + bit;
          if (y >= p->drawStartY && y <= p->drawEndY)
          {
            int16_t sy_rel = y - p->drawStartY;

            int16_t tx = (sx_rel * tex_w) / (p->drawEndX - p->drawStartX + 1);
            int16_t ty = (sy_rel * tex_h) / (p->drawEndY - p->drawStartY + 1);

            uint8_t img_page = ty >> 3;
            uint8_t img_bit = ty & 0x07;
            uint16_t didx = (uint16_t)img_page * tex_w + tx;

            if ((mask[didx] >> img_bit) & 0x01)
            {
              mask_byte |= (1 << bit);
              if ((data[didx] >> img_bit) & 0x01)
              {
                data_byte |= (1 << bit);
              }
            }
          }
        }

        if (mask_byte != 0)
        {
          uint16_t idx = (page << 7) + stripe;
          uint8_t bg = draw_buffer[idx];
          bg &= ~mask_byte;
          bg |= data_byte;
          draw_buffer[idx] = bg;
        }
      }
    }
  }
}

void DrawHud()
{
  int8_t weapon_bob_y, weapon_bob_x;
  if (is_moving)
  {
    weapon_bob_timer = (weapon_bob_timer + 1) & 15;
    weapon_bob_y = WEAPON_BOB[weapon_bob_timer];
    weapon_bob_x = WEAPON_BOB[(weapon_bob_timer + 4) & 15];
    if(weapon_bob_x == 2 || weapon_bob_x == -2)
    {
      UPI_SOUND_PLAY(240, 75);
    }
  }
  else
  {
    weapon_bob_y = 0;
    weapon_bob_x = 0;
  }

  sprite_proj_t weapon_proj = {0};

  int8_t recoil_offset = 0;
  if (shoot_cooldown > 10)
  {
    recoil_offset = 6;
  }
  else if (shoot_cooldown > 5)
  {
    recoil_offset = 3; // Плавное возвращение на место
  }

  weapon_proj.drawStartX = ((128 - HUD_WEAP_SIZE) / 2) + weapon_bob_x;
  weapon_proj.drawEndX = weapon_proj.drawStartX + HUD_WEAP_SIZE - 1;

  int16_t base_y = 58 - HUD_WEAP_SIZE - ABS8(weapon_bob_y) + recoil_offset;
  weapon_proj.drawStartY = base_y;
  weapon_proj.drawEndY = base_y + HUD_WEAP_SIZE - 1;

  // distInt = 0: оружие рисуется поверх стен (0 < zBuffer + 1 всегда)
  weapon_proj.distInt = 0;
  weapon_proj.is_visible = true;

  sprite_draw(&weapon_proj, HUD_WEAP_SIZE, HUD_WEAP_SIZE, SPRT_HUD_WEAP, MSK_HUD_WEAP);

  if (flash_timer > 0)
  {
    flash_timer--;

    // Вспышка — над центром оружия
    int16_t flash_x = weapon_proj.drawStartX + (HUD_WEAP_SIZE / 2) - (SPRT_FLASH_SIZE / 2);
    int16_t flash_y = weapon_proj.drawStartY - (SPRT_FLASH_SIZE / 2);

    sprite_proj_t flash_proj = {0};
    flash_proj.drawStartX = flash_x;
    flash_proj.drawEndX = flash_x + SPRT_FLASH_SIZE - 1;
    flash_proj.drawStartY = flash_y;
    flash_proj.drawEndY = flash_y + SPRT_FLASH_SIZE - 1;
    flash_proj.distInt = 0; // Тоже рисуем поверх всего
    flash_proj.is_visible = true;

    // SPRT_FLASH сам служит маской
    sprite_draw(&flash_proj, SPRT_FLASH_SIZE, SPRT_FLASH_SIZE, SPRT_FLASH, SPRT_FLASH);
  }

  if (doorInFront > -1)
  {
    if (!doors[doorInFront].is_open)
    {
      if (playerKeys > 0)
      {
        draw_bitmap(0, 48, 48, 8, LBL_4_OPEN, 0);
      }
      else
      {
        draw_bitmap(0, 48, 40, 8, LBL_NEED_KEY, 0);
      }
    }
  }

  draw_bitmap(0, 56, 128, 8, HUD_PANEL, 0);
  DrawNum(playerHealth, 25, 56);
  DrawNum(playerAmmo, 80, 56);
  DrawNum(playerKeys, 120, 56);
}

void UpdateEntities()
{
  if (player_hurt_timer > 0)
  {
    player_hurt_timer--;
  }

  for (int i = 0; i < COUNT_ENTITYS; i++)
  {
    entity_t *e = &entities[i];

    if (e->type == ENTITY_NONE || e->state < 1)
      continue;

    // вектор от врага к игроку
    fp_t dx = viewPlayer.x - e->x;
    fp_t dy = viewPlayer.y - e->y;

    // квадрат расстояния (без sqrt)
    int32_t dist_sq = (int32_t)FP_MUL(dx, dx) + (int32_t)FP_MUL(dy, dy);

    if (e->type == ENTITY_ENEMY)
    {
      if (e->hurt_timer == 0)
      {
        e->anim_timer++;
        if (e->anim_timer >= 8)
        {
          e->anim_timer = 0;
          e->anim_frame = (e->anim_frame + 1) & SPRT_ENEMY_COUNT;
        }
      }

      // движение
      // Если игрок дальше 1.5 клеток (чтобы не застревать в игроке)
      // и ближе 4 клеток (чтобы не бежать через всю карту)
      if (dist_sq > 150 && dist_sq < 4000)
      {
        fp_t next_x = e->x + (dx > 0 ? ENEMY_SPEED : -ENEMY_SPEED);
        fp_t next_y = e->y + (dy > 0 ? ENEMY_SPEED : -ENEMY_SPEED);

        int16_t check_x = FP_TO_INT(next_x);
        int16_t check_y = FP_TO_INT(e->y);

        // движение по X
        if (check_x >= 0 && check_x < LEVEL_SIZE && check_y >= 0 && check_y < LEVEL_SIZE)
        {
          if (Levels[MAPCELL(check_x, check_y)] != 1)
          {
            e->x = next_x;
          }
        }

        // перечитать X: он мог измениться выше
        check_x = FP_TO_INT(e->x);
        check_y = FP_TO_INT(next_y);

        // движение по Y
        if (check_x >= 0 && check_x < LEVEL_SIZE && check_y >= 0 && check_y < LEVEL_SIZE)
        {
          if (Levels[MAPCELL(check_x, check_y)] != 1)
          {
            e->y = next_y;
          }
        }
      }

      // атака в упор: квадрат расстояния < 300 (чуть больше клетки 256)
      if (dist_sq < 300)
      {
        if (has_line_of_sight(e->x, e->y, viewPlayer.x, viewPlayer.y))
        {
          // И игрок не в режиме неуязвимости
          if (player_hurt_timer == 0)
          {
            if (playerHealth > 10)
            {
              UPI_SOUND_PLAY(70, 100);
              UPI_SOUND_PLAY(90, 100);
              UPI_SOUND_PLAY(110, 100);
              playerHealth -= 10;
            }
            else
            {
              playerHealth = 0;
              Dead();
            }

            player_hurt_timer = 10; // 10 кадров неуязвимости с миганием
          }
        }
      }
    }
    else if (e->type == ENTITY_MEDKIT ||
             e->type == ENTITY_AMMO ||
             e->type == ENTITY_KEY)
    {

      // Если игрок подошёл вплотную (< 1 клетка)
      if (dist_sq < 200)
      {
        UPI_SOUND_PLAY(170, 75);
        UPI_SOUND_PLAY(150, 75);
        UPI_SOUND_PLAY(130, 75);
        // Подбираем аптечку, только если здоровье не полное
        if (e->type == ENTITY_MEDKIT)
        {
          if (playerHealth == 100)
          {
            continue;
          }
          if (playerHealth + 25 > 100)
          {
            playerHealth = 100;
          }
          else
          {
            playerHealth += 25;
          }
        }
        else if (e->type == ENTITY_AMMO)
        {
          if (playerAmmo == 200)
          {
            continue;
          }
          if (playerAmmo + 20 > 200)
          {
            playerAmmo = 200;
          }
          else
          {
            playerAmmo += 20;
          }
        }
        else if (e->type == ENTITY_KEY)
        {
          playerKeys += 1;
        }
        e->state = -1;
        pickup_flash_timer = 5;
      }
    }
    else if (e->type == ENTITY_EXIT)
    {
      if (dist_sq < 400)
      {
        UPI_SOUND_PLAY(190, 150);
        UPI_SOUND_PLAY(170, 150);
        UPI_SOUND_PLAY(150, 150);
        counter = 0;
        currentState = win;
        timer = 0;
        // game_state = STATE_VICTORY;
        e->state = -1; // Деактивируем
      }
    }
  }
}

void RenderEntitys()
{
  center_enemy_id = -1;
  center_enemy_dist = 32767;
  for (int i = 0; i < COUNT_ENTITYS; i++)
  {
    entity_t *e = &entities[i];

    if (e->type == ENTITY_NONE || e->state < 0)
    {
      continue;
    }

    const uint8_t *data = NULL;
    const uint8_t *mask = NULL;
    uint8_t size = 0;

    switch (e->type)
    {
    case ENTITY_ENEMY:
      if (e->state == 0)
      {
        // мёртвый враг
        data = &SPRT_ENEMY[SPRT_ENEMY_DEAD_NUM * SPRT_ENEMY_OFFSET];
        mask = &MSK_ENEMY[SPRT_ENEMY_DEAD_NUM * SPRT_ENEMY_OFFSET];
      }
      else
      {
        if (e->hurt_timer > 0)
        {
          // Раненый
          e->hurt_timer--;
          data = &SPRT_ENEMY[SPRT_ENEMY_HURT_NUM * SPRT_ENEMY_OFFSET];
          mask = &MSK_ENEMY[SPRT_ENEMY_HURT_NUM * SPRT_ENEMY_OFFSET];
        }
        else
        {
          uint16_t offset = (e->anim_frame) * SPRT_ENEMY_OFFSET;
          data = &SPRT_ENEMY[offset];
          mask = &MSK_ENEMY[offset];
        }
      }
      size = SPRT_ENEMY_SIZE;
      break;

    case ENTITY_MEDKIT:
      data = SPRT_ITEM_MEDKIT;
      mask = MSK_ITEM_MEDKIT;
      size = SPRT_MEDKIT_SIZE;
      break;

    case ENTITY_AMMO:
      data = SPRT_ITEM_AMMO;
      mask = MSK_ITEM_MEDKIT;
      size = SPRT_AMMO_SIZE;
      break;

    case ENTITY_KEY:
      data = SPRT_ITEM_KEY;
      mask = MSK_ITEM_KEY;
      size = SPRT_KEY_SIZE;
      break;

    case ENTITY_EXIT:
      data = SPRT_EXIT;
      mask = MSK_EXIT;
      size = SPRT_EXIT_SIZE;
      break;

    default:
      continue;
    }

    if (data == NULL || size == 0)
    {
      continue;
    }

    sprite_proj_t proj = sprite_project(e->x, e->y, size);
    if (!proj.is_visible)
    {
      continue;
    }

    if (e->type == ENTITY_ENEMY && e->state > 0)
    {
      if (proj.drawStartX <= 64 && proj.drawEndX >= 64)
      {
        // враг виден (не за стеной) и ближе предыдущего кандидата
        if (proj.distInt < zBuffer[64] + 1 && proj.distInt < center_enemy_dist)
        {
          center_enemy_id = i;
          center_enemy_dist = proj.distInt;
        }
      }
    }

    sprite_draw(&proj, size, size, data, mask);
  }
}

void player_shoot()
{
  if (playerAmmo == 0)
  {
    return;
  }
  playerAmmo--;

  UPI_SOUND_PLAY(100, 20);
  UPI_SOUND_PLAY(80, 20);

  shoot_cooldown = 15;
  flash_timer = 3;

  if (center_enemy_id != -1)
  {
    entities[center_enemy_id].state -= 1;
    entities[center_enemy_id].hurt_timer = 8;
  }
}

void render_damage_feedback()
{
  if (player_hurt_timer > 0)
  {
    // Мигание, пока у таймера установлен бит 2 (каждые 4 кадра)
    if ((player_hurt_timer >> 2) & 1)
    {
      for (int i = 0; i < 1024; i++)
        draw_buffer[i] = ~draw_buffer[i];
    }
  }
}

void Dead()
{
  counter = 0;
  currentState = dead;
  clearFlag = false;
}

void shift_column_down_1px(uint16_t col)
{
  // Проходим по страницам снизу вверх
  for (int8_t page = 7; page >= 0; page--)
  {
    uint16_t idx = (page << 7) + col;

    if (page == 7)
    {
      // Последняя страница: просто сдвигаем вниз на 1 бит
      draw_buffer[idx] >>= 1;
    }
    else
    {
      uint8_t current = draw_buffer[idx];
      uint8_t next_page = draw_buffer[idx + 128]; // страница ниже

      // сдвиг вниз на 1 бит + перенос старшего бита из следующей страницы
      draw_buffer[idx] = (current >> 1) | ((next_page & 0x01) << 7);
    }
  }

  // верхний пиксель (page 0, бит 0) — белый
  draw_buffer[col] |= 0x01;
}

void DrawDead()
{
  clearFlag = false;
  counter++;

  // сдвигаем экран вниз каждые 2 кадра
  if (counter % 2 == 0)
  {
    if (counter >= 256)
    {
      counter = 0;
      currentState = gameover;
      return;
    }

    // столбцы, до которых дошло падение
    for (uint16_t col = 0; col < counter && col < 128; col++)
    {
      shift_column_down_1px(col);
    }

    SetDirty();
  }
}

void render_pickup_feedback()
{
  if (pickup_flash_timer > 0)
  {
    pickup_flash_timer--;

    // инвертируем нижнюю панель (page 7)
    if (pickup_flash_timer & 1)
    {
      for (int16_t x = 0; x < 128; x++)
      {
        draw_buffer[(7 << 7) + x] = ~draw_buffer[(7 << 7) + x];
      }
    }
  }
}

// Возвращает индекс двери перед игроком, или -1 если двери нет
int8_t check_door_in_front()
{
  fp_t front_x = viewPlayer.x + (DIR_COS[viewPlayer.angle]);
  fp_t front_y = viewPlayer.y + (DIR_SIN[viewPlayer.angle]);

  uint8_t fx = FP_TO_INT(front_x);
  uint8_t fy = FP_TO_INT(front_y);

  if (fx >= LEVEL_SIZE || fy >= LEVEL_SIZE)
    return -1;

  uint8_t cell = Levels[MAPCELL(fx, fy)];

  // 8 = дверь
  if (cell == 8)
  {
    for (uint8_t i = 0; i < door_count; i++)
    {
      if (doors[i].x == fx && doors[i].y == fy && !doors[i].is_open)
      {
        return i;
      }
    }
  }

  return -1;
}

bool can_pass_through(int16_t mapX, int16_t mapY)
{
  if (mapX < 0 || mapX >= LEVEL_SIZE || mapY < 0 || mapY >= LEVEL_SIZE)
  {
    return false;
  }

  uint8_t cell = Levels[MAPCELL(mapX, mapY)];

  // Непроходимы: стена (1) и закрытая дверь (8)
  if (cell != 1 && cell != 8)
  {
    return true;
  }

  if (cell == 8)
  {
    for (uint8_t i = 0; i < door_count; i++)
    {
      if (doors[i].x == mapX && doors[i].y == mapY)
      {
        return doors[i].is_open;
      }
    }
  }

  return false;
}

void StateWin()
{
  uint8_t freq = doom_melody[counter][0];
  uint8_t ticks = doom_melody[counter][1];
  if (ticks > 0)
  {
    UPI_SOUND_PLAY(freq, ticks * 75);
  }
  counter++;
  if (doom_melody[counter][0] == 0 && doom_melody[counter][1] == 0)
  {
    counter = 0;
  }

  draw_bitmap(48, 8, 32, 8, LBL_WIN, 0);

  timer += deltaTime;
  if (timer > 750)
  {
    blinker = !blinker;
    timer -= 750;
  }
  if (blinker)
    draw_bitmap(60, 56, 8, 8, ICON_BTN_ACT1, 0);
  else
  {
    draw_bitmap(56, 52, 16, 16, ICON_BTN_ACT1_PRESSED, 0);
  }

  if (btn_act1_wasPressedThisFrame)
  {
    currentState = title;
  }
}

bool has_line_of_sight(fp_t fromX, fp_t fromY, fp_t toX, fp_t toY)
{
  fp_t dx = toX - fromX;
  fp_t dy = toY - fromY;

  // Количество шагов = максимальное расстояние по осям
  int steps = FP_ABS(dx) > FP_ABS(dy) ? FP_ABS(dx) : FP_ABS(dy);
  steps >>= 8; // Переводим в клетки
  if (steps == 0)
    steps = 1;
  if (steps > 16)
    steps = 16; // Ограничение для производительности

  fp_t stepX = dx / steps;
  fp_t stepY = dy / steps;

  fp_t checkX = fromX;
  fp_t checkY = fromY;

  for (int i = 0; i < steps; i++)
  {
    checkX += stepX;
    checkY += stepY;

    int16_t mapX = FP_TO_INT(checkX);
    int16_t mapY = FP_TO_INT(checkY);

    if (mapX >= 0 && mapX < LEVEL_SIZE && mapY >= 0 && mapY < LEVEL_SIZE)
    {
      uint8_t cell = Levels[MAPCELL(mapX, mapY)];
      // Стена (1) или враг (2) — видимости нет
      // Замечание: закрытая дверь (8) пока не блокирует луч обзора
      if (cell == 1 || cell == 2)
      {
        return false;
      }
    }
  }

  return true; // Путь чист
}
