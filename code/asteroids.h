#ifndef ASTEROIDS_H
#define ASTEROIDS_H

#include "renderer.h"

enum EntityType
{
  EntityType_NONE,
  EntityType_PLAYER,
  EntityType_ASTEROID,
  EntityType_BULLET,
};

struct Particle
{
  Transform t;
  
  v2 d_p;
  v2 d_scale;
};

struct ParticleSystem
{
  Particle *items;
  u32 items_capacity;
  u32 items_count;
  
  RandomSequence seed;
};

struct EntityPlayer
{
  f32 shot_cooldown;
};

struct EntityBullet
{
  f32 lifetime;
};

struct EntityAsteroid
{
  f32 scale;
};

struct Entity
{
  b32 is_temporary;
  b32 exists;
  EntityType type;
  
  ShapePolygon shape;
  Transform t;
  
  v2 velocity;
  f32 angular_velocity;
  
  union
  {
    EntityPlayer player;
    EntityBullet bullet;
    EntityAsteroid asteroid;
  };
};

struct State
{
  u32 asteroids_per_wave;
  u32 asteroid_count;
  
  RenderGroup render_group;
  f32 screenshake_timer;
  
  ParticleSystem particle_system;
  
  Entity entities[1024];
  u32 entities_count;
  
  b32 initialized;
  
  Arena arena;
  Arena transient_arena;
  
  u32 shader;
  v2 game_area_size;
  RandomSequence seed;
};


#endif ASTEROIDS_H