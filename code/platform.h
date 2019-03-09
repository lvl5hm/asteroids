#ifndef PLATFORM_H
#define PLATFORM_H

#include "utils.h"

struct GameMemory
{
  byte *data;
  u64 size;
};


struct Button
{
  b32 is_down;
  b32 went_down;
  b32 went_up;
};

struct GameInput
{
  union {
    Button buttons[4];
    struct
    {
      Button up;
      Button left;
      Button right;
      Button space;
    };
  };
  
  f32 delta_time;
};

struct GameScreen
{
  v2 size;
};

String platform_read_entire_file(String file_name);

#define GAME_UPDATE(name) void name(GameMemory *memory, GameInput *input, GameScreen *screen)
typedef GAME_UPDATE(type_game_update);
type_game_update game_update;


#endif