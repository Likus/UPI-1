// ===================================================================================
// Project:   Snake Game
// Version:   v1.0
// Year:      2024
// Author:    Dmitriy Stukalov aka Terrabyte
// Dzen:      https://dzen.ru/terrabyte
// VK:        https://vk.com/terrabyte
// VK-Video:  https://vk.com/video/@terrabyte
// Based on:  Stefan Wagner MCU-Templates, https://github.com/wagiminator/MCU-Templates
// License:   http://creativecommons.org/licenses/by-sa/3.0/
// ===================================================================================

// ===================================================================================
// Описание
// ===================================================================================

// Игра "Змейка".
// Вы управляете змейкой, которая ползает и кушает яблоки. С каждым яблоком ее длина увеличивается.
// Змейка не должна упереться в стену и не должна наткнуться на себя саму.
// Задача - съесть как можно больше яблок.
// Управление: Клавиша "Влево" - поворот змейки в левую сторону. Клавиша "Вправо" - поворот змейки в правую сторону.

// ===================================================================================
// Определения и глобальные переменные
// ===================================================================================

#include "driver.h"     // Функции управления интерфейсными устройствами (клавиатура, звук, экран)
#include "spritebank.h" // Спрайты игры

// Режимы игры
#define GAME_INTRO 0x00 // Заставка, ожидание нажатия на кнопку
#define GAME_PLAY 0x01  // Проигрывание звука
#define GAME_OVER 0x02  // Игра завершена

// Временные константы
#define TICKS_IN_MS ((uint32_t)(F_CPU / 1000)) // Сколько тиков в одной миллисекунде
#define SNAKE_START_TIME_MS 300                // Начальный период перемещения змейки (мс)
#define BUTTON_READ_TIME 20                    // Период опроса клавиатуры (мс)

#define JOY_OLED_data_startY(y) \
  {                             \
    OLED_setpos(0, y);          \
    OLED_data_start();          \
  }
#define JOY_OLED_data_startXY(x, y) \
  {                                 \
    OLED_setpos(x, y);              \
    OLED_data_start();              \
  } // Добавил для простоты адаптации
// Добавил функции для более точного упрвления временем
#define JOY_GET_TICK() (STK->CNT) // Запрос текущего значения системного таймера

// Общие переменные игры
uint8_t game_mode = GAME_INTRO; // Текущий режим игры

// Переменные змейки
uint32_t snake_time_act = SNAKE_START_TIME_MS; // Время в мс, определяющее скорость змейки (через какое время змейка смещается на 1 клетку). Эта переменная нужна если в будущем будет реализован механизм ускорения
uint32_t snake_tick_target;                    // Время в тиках таймера, когда змейка должна сделать ход
uint8_t snake[32];                             // Буфер змейки. Каждый байт содержит сведения о 4-х сегментах змеи (2 бита).
                                               // Положение сегмента задается смещением относительно предыдущего (00 - влево, 01 - вверх, 10 - вниз, 11 - вправо)
                                               // Максимальная длина змейки 32х4=128 сегментов. Максимальное число ячеек на экране 16x8=128
uint8_t snake_head_x;                          // Координата X головы змейки
uint8_t snake_head_y;                          // Координата Y головы
uint8_t snake_tail_x;                          // Координата X хвоста
uint8_t snake_tail_y;                          // Координата Y хвоста
uint8_t snake_head_pos;                        // Позиция головы (соответствующей ей пары битов) в массиве змейки. Макимальное значение - 128.
uint8_t snake_tail_pos;                        // Позиция хвоста (пары битов) в массиве змейки

// Переменные яблока
uint8_t apple_x; // Координата X кролика
uint8_t apple_y; // Координата Y кролика

// Переменные клавиатуры
uint8_t button_left_fixed = 0;  // Зафиксировано нажатие на левую кнопку
uint8_t button_right_fixed = 0; // Зафиксировано нажатие на праавую кнопку
uint8_t button_up_fixed = 0;
uint8_t button_down_fixed = 0;
uint8_t button_left_old_state = 0; // Состояние кнопки во время прошлого опроса. Если ранее уже была нажата, то нажатие не обрабатываем
uint8_t button_right_old_state = 0;
uint8_t button_up_old_state = 0;
uint8_t button_down_old_state = 0;
uint32_t button_tick_target; // Время в тиках таймера, когда надо опросить клавиатуру в следующий раз

uint8_t score = 0; // Результат игры

// Прототипы функций

void SCREEN_show_intro(void);
void SCREEN_show_ground(void);
void SCREEN_draw_segment(uint8_t x, uint8_t y, uint8_t color);
void SCREEN_clear(void);
void SCREEN_draw_digit(uint8_t digit, uint8_t pos_x, uint8_t pos_y);
void SCREEN_draw_result(void);
void GAME_start(void);
void GAME_finish(void);
void GAME_reset(void);
void SNAKE_init(void);
uint8_t SNAKE_collision(uint8_t check_x, uint8_t check_y);
uint8_t SNAKE_get_bits(uint8_t pos);
void SNAKE_set_bits(uint8_t pos, uint8_t bits);
void SNAKE_draw_head(void);
void SNAKE_draw_body(void);
void SNAKE_draw_tail(void);
void APPLE_generate_position(void);
void APPLE_draw(void);
void BUTTON_init();

// ===================================================================================
// Главная функция
// ===================================================================================

int main(void)
{
  // Инициализация кнопок джойстика
  JOY_init();
  // Вывод заставки
  SCREEN_show_intro();
  while (1)
  {
    switch (game_mode)
    {
    // Режим экранной заставки *****************************************************
    case GAME_INTRO:
    {
      // Ожидаем нажатия на клавишу ACT и запускаем игру
      if (JOY_act_pressed())
        GAME_start();
    }
    break;
    // Режим основной игры *********************************************************
    case GAME_PLAY:
    {
      // Не пора ли опросить клавиатуру
      if ((int32_t)(JOY_GET_TICK() - button_tick_target) >= 0)
      {
        button_tick_target = JOY_GET_TICK() + (BUTTON_READ_TIME * TICKS_IN_MS);
        // Фиксируем нажатие на кнопку. Нажатие сбрасывается после его обработки змейкой
        if (JOY_left_pressed())
        {
          // Убедимся, что левая клавиша не была нажата раньше
          if (!button_left_old_state)
          {
            button_left_fixed = 1;
            button_right_fixed = 0;
            button_left_old_state = 1;
          }
        }
        else
        {
          button_left_old_state = 0;
        }
        if (JOY_right_pressed())
        {
          if (!button_right_old_state)
          {
            button_left_fixed = 0;
            button_right_fixed = 1;
            button_right_old_state = 1;
          }
        }
        else
        {
          button_right_old_state = 0;
        }
        if (JOY_up_pressed())
        {
          if (!button_up_old_state)
          {
            button_down_fixed = 0;
            button_up_fixed = 1;
            button_up_old_state = 1;
          }
        }
        else
        {
          button_up_old_state = 0;
        }
        if (JOY_down_pressed())
        {
          if (!button_down_old_state)
          {
            button_up_fixed = 0;
            button_down_fixed = 1;
            button_down_old_state = 1;
          }
        }
        else
        {
          button_down_old_state = 0;
        }
      } // Закончили с клавиатурой
      // Не пора ли змейке сделать шаг
      if ((int32_t)(JOY_GET_TICK() - snake_tick_target) >= 0)
      {
        snake_tick_target = JOY_GET_TICK() + (snake_time_act * TICKS_IN_MS); // Определяем время следующего шага
        // Пора двинуться на 1 шаг
        uint8_t bits;
        bits = SNAKE_get_bits(snake_head_pos); // Получаем биты направления головы

        if (bits == 0b00000000 || bits == 0b00000010)
        {
          if (button_up_fixed)
          {
            bits = 0b00000011;
          }
          if (button_down_fixed)
          {
            bits = 0b00000001;
          }
        }
        else
        {
          if (button_left_fixed)
          {
            bits = 0b00000010;
          }
          if (button_right_fixed)
          {
            bits = 0b00000000;
          }
        }

        // Если была нажата клавиша, изменяем направление движения
        //if (button_left_fixed)
        //  bits = ((bits - 1) & 0x03);
        //if (button_right_fixed)
        //  bits = ((bits + 1) & 0x03);
        // Сбрасываем фиксацию клавиш
        button_left_fixed = 0;
        button_right_fixed = 0;
        button_down_fixed = 0;
        button_up_fixed = 0;
        // Вместо головы отрисовываем туловище
        SNAKE_draw_body(); // Отрисовка последующего сегмента (где раньше была голова)
        // Вычисляем новые координаты головы (биты направления движения головы интерпретируются в другую сторону, чем относительные биты сегментов туловища)
        if (bits == 0b00000000)
        {
          snake_head_x++;
        }
        if (bits == 0b00000001)
        {
          snake_head_y++;
        }
        if (bits == 0b00000010)
        {
          snake_head_x--;
        }
        if (bits == 0b00000011)
        {
          snake_head_y--;
        }
        // Сохраняем новые биты направления в голове
        SNAKE_set_bits(snake_head_pos, bits);
        // Изменяем индекс головы в массиве. При достижении конца массива перескакиваем в начало
        snake_head_pos = (snake_head_pos + 1) & 0x7F;
        // В новую позицию заносим такие же биты
        SNAKE_set_bits(snake_head_pos, bits);
        // Проверка столкновений со стенками
        if (snake_head_x & 0xF0)
        {
          GAME_finish();
          goto GAME_PLAY_END;
        } // при переполнении координаты - столкнулись со стенкой (16 ячеек по горизонтали)
        if (snake_head_y & 0xF8)
        {
          GAME_finish();
          goto GAME_PLAY_END;
        }                  // 8 ячеек по вертикали
        SNAKE_draw_head(); // Отрисовка головы
        // Проверка на яблоко
        if ((snake_head_x == apple_x) && (snake_head_y == apple_y))
        {
          // Если яблоко съели, то генерируем новое яблоко
          APPLE_generate_position();
          // Увеличиваем результат
          score++;
          // Издаем звук
          JOY_sound(64, 192);
          JOY_sound(128, 255);
        }
        else
        {
          // Если кролика не съели, то подтягиваем хвост
          SCREEN_draw_segment(snake_tail_x, snake_tail_y, 0x00); // Удаляется хвост
          // Теперь можно хвост подтянуть
          bits = SNAKE_get_bits(snake_tail_pos); // Получение битов направления
          // Изменение координат хвоста
          if (bits == 0b00000000)
            snake_tail_x++;
          if (bits == 0b00000001)
            snake_tail_y++;
          if (bits == 0b00000010)
            snake_tail_x--;
          if (bits == 0b00000011)
            snake_tail_y--;
          snake_tail_pos = (snake_tail_pos + 1) & 0x7F; // Продвижение позиции хвоста в массиве змеи
          // Рисуем хвост
          SNAKE_draw_tail();
        }
        // Проверка, что змейка не уперлась в саму себя
        if (SNAKE_collision(snake_head_x, snake_head_y))
          GAME_finish(); // Завершение игры если произошло столкновение
      }
    GAME_PLAY_END:;
    }
    break;
    // Режим игра окончена *********************************************************
    case GAME_OVER:
    {
      // Ожидаем нажатия на клавишу ACT и запускаем игру
      if (JOY_act_pressed())
        GAME_reset();
    }
    break;
    default:
      break;
    }
  }
}

// ===================================================================================
// Функции работы с экраном
// ===================================================================================

// Вывод заставки на экран
void SCREEN_show_intro(void)
{
  uint8_t y, x;
  for (y = 0; y < 8; y++)
  {
    JOY_OLED_data_startY(y);
    for (x = 0; x < 128; x++)
    {
      JOY_OLED_send((BITMAP_INTRO[x + (y * 128)]));
    }
    JOY_OLED_end();
  }
}

void SCREEN_show_ground(void)
{
  uint8_t y, x;
  for (y = 0; y < 8; y++)
  {
    JOY_OLED_data_startY(y);
    for (x = 0; x < 128; x++)
    {
      JOY_OLED_send((BITMAP_GROUND[x + (y * 128)]));
    }
    JOY_OLED_end();
  }
}

// Вывод или затирание сегмента (квадратика 8х8 пикселей) по заданным столбцу и строке (всего 16 ячеек по горизонтали: x = 0...15 и 8 по вертикали y = 0...7)
void SCREEN_draw_segment(uint8_t x, uint8_t y, uint8_t color)
{
  x <<= 3;
  JOY_OLED_data_startXY(x, y);
  for (uint8_t i = 8; i; i--)
  {
    JOY_OLED_send(BITMAP_GROUND[x + (y * 128) + 8 - i]);
  }
  JOY_OLED_end();
}

// Очистка экрана
void SCREEN_clear(void)
{
  for (uint8_t y = 0; y < 8; y++)
  {
    JOY_OLED_data_startY(y);
    for (uint8_t x = 0; x < 128; x++)
    {
      JOY_OLED_send(0x00);
    }
    JOY_OLED_end();
  }
}

// Вывод десятичной цифры размером 16x24 по заданным координатам
void SCREEN_draw_digit(uint8_t digit, uint8_t pos_x, uint8_t pos_y)
{
  for (uint8_t y = 0; y < 3; y++)
  {
    JOY_OLED_data_startXY(pos_x << 3, pos_y + y);
    for (uint8_t x = 0; x < 16; x++)
    {
      JOY_OLED_send(DIGITS[digit * 16 + 160 * y + x]);
    }
    JOY_OLED_end();
  }
}

// Вывод результата
void SCREEN_draw_result(void)
{
  // Вывод заставки с надписью SCORE
  uint8_t y, x;
  for (y = 0; y < 4; y++)
  {
    JOY_OLED_data_startY(y);
    for (x = 0; x < 128; x++)
    {
      JOY_OLED_send((BITMAP_SCORE[x + (y * 128)]));
    }
    JOY_OLED_end();
  }
  // Вывод результата
  uint8_t d;
  uint8_t pos;
  // Определение позиции вывода в зависимости от размера числа
  if (score < 10)
    pos = 7;
  else if (score < 100)
    pos = 8;
  else
    pos = 9;
  // По очереди выводим все десятичные цифры
  do
  {
    d = score % 10;
    SCREEN_draw_digit(d, pos, 4);
    pos -= 2;
    score /= 10;
  } while (score > 0);
}

// ===================================================================================
// Функции управления игрой
// ===================================================================================

// Запуск игры
void GAME_start(void)
{
  game_mode = GAME_PLAY;
  SCREEN_show_ground();      // Очистка игрового поля
  JOY_DLY_ms(1000);          // Задержка
  SNAKE_init();              // Установка начальных параметров змейки
  BUTTON_init();             // Начальные параметры опроса клавиатуры
  APPLE_generate_position(); // Начальное положение кролика
}

// Завершение игры
void GAME_finish(void)
{
  JOY_sound(128, 255);
  JOY_sound(64, 192);
  JOY_sound(1, 128);
  JOY_DLY_ms(1000); // Задержка 2 секунды
  game_mode = GAME_OVER;
  SCREEN_clear();
  SCREEN_draw_result();
}

// Перезапуск игры
void GAME_reset(void)
{
  game_mode = GAME_INTRO;
  SCREEN_show_intro();
  JOY_DLY_ms(1000); // Задержка
}

// ===================================================================================
// Функции управления змейкой
// ===================================================================================

// Установка начальных параметров змейки
void SNAKE_init(void)
{
  snake_time_act = SNAKE_START_TIME_MS;                          // Период времени, через который змейка движется (в мс - так удобнее)
  snake_tick_target = STK->CNT + (snake_time_act * TICKS_IN_MS); // Установка целевого времени змейки. При его достижении змейка должна будет сдвинется
  // Установка начальных координат головы и хвоста
  snake_head_x = 2;
  snake_head_y = 4;
  snake_tail_x = 0;
  snake_tail_y = 4;
  // Установка указателей головы и хвоста
  snake[0] = 0;       // В первый байт змейки заносится 0 - движение вправо
  snake_head_pos = 2; // Номер сегмента головы
  snake_tail_pos = 0; // Номер сегмента хвоста
  score = 0;
}

// Проверка, есть ли по заданным координатам тело змейки
uint8_t SNAKE_collision(uint8_t check_x, uint8_t check_y)
{
  uint8_t pos;
  uint8_t x, y;
  uint8_t bits; // Два бита, хранящие относительную информацию о сегменте змейки
  pos = snake_tail_pos;
  x = snake_tail_x;
  y = snake_tail_y;
  // Цикл о тхвоста по всем элементам змейки. Пока не дойдем до головы. Сама голова не проверяется - она в себя врезаться не может
  while (pos != snake_head_pos)
  {
    if ((x == check_x) && (y == check_y))
      return 1;
    // Получаем координаты следующего сегмента змейки
    bits = SNAKE_get_bits(pos); // Получение битов направления
    // Интерпретация битов направления
    if (bits == 0b00000000)
      x++;
    if (bits == 0b00000001)
      y++;
    if (bits == 0b00000010)
      x--;
    if (bits == 0b00000011)
      y--;
    pos = (pos + 1) & 0x7F;
  }
  return 0;
}

// Получение значащих битов, соответствующих номеру сегмента змейки
uint8_t SNAKE_get_bits(uint8_t pos)
{
  return (snake[pos >> 2] >> ((pos & 0x03) << 1)) & 0x03;
}

// Установка битов в массиве змейки по заданной позиции сегмента тела
void SNAKE_set_bits(uint8_t pos, uint8_t bits)
{
  snake[pos >> 2] = snake[pos >> 2] & (~(0x03 << ((pos & 0x03) << 1))); // Обнуляем биты направления в массиве
  snake[pos >> 2] = snake[pos >> 2] | (bits << ((pos & 0x03) << 1));    // Заносим новые биты
}

// Отрисовка головы змейки
void SNAKE_draw_head(void)
{
  uint8_t x = snake_head_x << 3;
  JOY_OLED_data_startXY(x, snake_head_y);
  for (uint8_t i = 8; i; i--)
  {
    JOY_OLED_send(SNAKE_HEAD_BITMAP[SNAKE_get_bits(snake_head_pos)][8 - i] | BITMAP_GROUND[x + (snake_head_y * 128) + 8 - i]);
  }
  JOY_OLED_end();
}

// Отрисовка тела змейки
void SNAKE_draw_body(void)
{
  uint8_t x = snake_head_x << 3;
  JOY_OLED_data_startXY(x, snake_head_y);
  for (uint8_t i = 8; i; i--)
  {
    JOY_OLED_send(SNAKE_BODY_BITMAP[8 - i] | BITMAP_GROUND[x + (snake_head_y * 128) + 8 - i]);
  }
  JOY_OLED_end();
}

// Отрисовка хвоста змейки
void SNAKE_draw_tail(void)
{
  uint8_t x = snake_tail_x << 3;
  JOY_OLED_data_startXY(x, snake_tail_y);
  for (uint8_t i = 8; i; i--)
  {
    JOY_OLED_send(SNAKE_TAIL_BITMAP[SNAKE_get_bits(snake_tail_pos)][8 - i] | BITMAP_GROUND[x + (snake_tail_y * 128) + 8 - i]);
  }
  JOY_OLED_end();
}

// ===================================================================================
// Функции управления яблоком
// ===================================================================================

// Генерация псевдослучайной позиции яблока
void APPLE_generate_position(void)
{
  // Для получения случайных координат используется значение системного таймера
  apple_x = STK->CNT & 0x0F;
  apple_y = STK->CNT & 0x07;
  // Если новое положение приходится на змейку, то постепенно увеличиваем координаты пока не найдем свободное место
  while ((SNAKE_collision(apple_x, apple_y) || ((snake_head_x == apple_x) && (snake_head_y == apple_y))))
  {
    apple_x = (apple_x + 1) & 0x0F;
    if (apple_x == 0)
      apple_y = (apple_y + 1) & 0x07;
  }
  APPLE_draw(); // Отрисовка яблока
}

// Вывод яблока на экран
void APPLE_draw(void)
{
  uint8_t x = apple_x << 3;
  JOY_OLED_data_startXY(x, apple_y);
  for (uint8_t i = 8; i; i--)
  {
    JOY_OLED_send(APPLE_BITMAP[8 - i] | BITMAP_GROUND[x + (apple_y * 128) + 8 - i]);
  }
  JOY_OLED_end();
}

// ===================================================================================
// Функции управления клавиатурой
// ===================================================================================

// Инициализация клавиатуры
void BUTTON_init()
{
  button_left_fixed = 0;
  button_right_fixed = 0;
  button_tick_target = STK->CNT + (BUTTON_READ_TIME * TICKS_IN_MS);
  button_left_old_state = 0;
  button_right_old_state = 0;
}
