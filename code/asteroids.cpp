#include "asteroids.h"


#define SHIP_SPEED_LIMIT 8
#define BULLET_SPEED_LIMIT 16

/*

TODO:
-gui (font rendering?)
-sound?
-porting to linux?

*/

#define COLOR_WHITE v4(1, 1, 1, 1)
#define COLOR_RED v4(1, 0, 0, 1)


rect2 polygon_to_aabb(ShapePolygon poly)
{
  rect2 result = inverted_infinity_rect2();
  for (u32 vertex_index = 0;
       vertex_index < poly.count;
       vertex_index++)
  {
    v2 v = poly.vertices[vertex_index];
    if (v.x < result.min.x)
    {
      result.min.x = v.x;
    }
    if (v.y < result.min.y)
    {
      result.min.y = v.y;
    }
    if (v.x > result.max.x)
    {
      result.max.x = v.x;
    }
    if (v.y > result.max.y)
    {
      result.max.y = v.y;
    }
  }
  
  return result;
}

void push_transient_context(State *state)
{
  LocalContext ctx = make_context(get_local_context());
  ctx.allocator = arena_allocator;
  ctx.allocator_data = &state->transient_arena;
  push_context(ctx);
}


void sort_insertion(f32 *array, i32 count)
{
  for (i32 i = 1; i < count; i++)
  {
    i32 j = i;
    while (j > 0 && array[j-1] > array[j])
    {
      swap(array[j], array[j-1]);
      j--;
    }
  }
}

void shuffle_array(RandomSequence *rand, f32 *array, i32 count)
{
  for (i32 i = 1; i < count; i++)
  {
    i32 j = i;
    while (j > 0 && random(rand) > 0.5f)
    {
      swap(array[j], array[j-1]);
      j--;
    }
  }
}

ShapePolygon generate_random_convex_polygon(RandomSequence *rand, u32 count, f32 scale)
{
  assert(count <= array_count(((ShapePolygon *)0)->vertices));
  f32 *x_coords = temp_alloc_array(f32, count);
  f32 *y_coords = temp_alloc_array(f32, count);
  
  
  // NOTE(lvl5): 1) generate lists of x and y coordinates
  for (u32 i = 0; i < count; i++)
  {
    x_coords[i] = random(rand)*scale;
    y_coords[i] = random(rand)*scale;
  }
  
  // NOTE(lvl5): sort them
  sort_insertion(x_coords, count);
  sort_insertion(y_coords, count);
  
  // NOTE(lvl5): get exterior points
  f32 x_min = x_coords[0];
  f32 x_max = x_coords[count-1];
  
  f32 y_min = y_coords[0];
  f32 y_max = y_coords[count-1];
  
  // NOTE(lvl5): randomly pair up interior points into two chains
  // and extract vector components
  f32 *x_components = temp_alloc_array(f32, count);
  u32 x_components_count = 0;
  
  f32 x_last_top = x_min;
  f32 x_last_bottom = x_min;
  
  for (u32 i = 1; i < count-1; i++)
  {
    f32 comp;
    if (random(rand) > 0.5f)
    {
      comp = x_coords[i] - x_last_top;
      x_last_top = x_coords[i];
    }
    else
    {
      comp = x_last_bottom - x_coords[i];
      x_last_bottom = x_coords[i];
    }
    
    x_components[x_components_count++] = comp;
  }
  x_components[x_components_count++] = x_max - x_last_top;
  x_components[x_components_count++] = x_last_bottom - x_max;
  
  
  f32 *y_components = temp_alloc_array(f32, count);
  u32 y_components_count = 0;
  
  f32 y_last_top = y_min;
  f32 y_last_bottom = y_min;
  
  for (u32 i = 1; i < count-1; i++)
  {
    f32 comp;
    if (random(rand) > 0.5f)
    {
      comp = y_coords[i] - y_last_top;
      y_last_top = y_coords[i];
    }
    else
    {
      comp = y_last_bottom - y_coords[i];
      y_last_bottom = y_coords[i];
    }
    
    y_components[y_components_count++] = comp;
  }
  y_components[y_components_count++] = y_max - y_last_top;
  y_components[y_components_count++] = y_last_bottom - y_max;
  
  
  // NOTE(lvl5): pair up components into vectors randomly
  shuffle_array(rand, x_components, x_components_count);
  shuffle_array(rand, y_components, y_components_count);
  
  v2 *vectors = temp_alloc_array(v2, count);
  for (u32 i = 0; i < count; i++)
  {
    vectors[i] = v2(x_components[i], y_components[i]);
  }
  
  // NOTE(lvl5): sort vectors by angle
  for (u32 i = 0; i < count; i++)
  {
    u32 j = i;
    while (j > 0 && get_angle(vectors[j-1]) > get_angle(vectors[j]))
    {
      swap(vectors[j], vectors[j-1]);
      j--;
    }
  }
  
  
  ShapePolygon result;
  result.vertices[0] = v2(0, 0);
  result.count = 1;
  
  v2 moved_min = v2(F32_MAX, F32_MAX);
  v2 moved_max = v2(F32_MIN, F32_MIN);
  
  for (u32 i = 1; i < count; i++)
  {
    v2 prev = result.vertices[i-1];
    v2 new_v = prev + vectors[i-1];
    result.vertices[result.count++] = new_v;
    
    if (new_v.x < moved_min.x)
      moved_min.x = new_v.x;
    if (new_v.y < moved_min.y)
      moved_min.y = new_v.y;
    
    if (new_v.x > moved_max.x)
      moved_max.x = new_v.x;
    if (new_v.y > moved_max.y)
      moved_max.y = new_v.y;
  }
  
  v2 center = moved_min + (moved_max - moved_min)/2;
  
  for (u32 i = 0; i < count; i++)
  {
    result.vertices[i] -= center;
  }
  
  return result;
}


Entity *add_entity(State *state, EntityType type)
{
  assert(state->entities_count < array_count(state->entities));
  u32 index = state->entities_count++;
  Entity *e = state->entities + index;
  *e = {};
  e->t.scale = v2(1, 1);
  e->exists = true;
  e->type = type;
  e->is_temporary = false;
  
  return e;
}

#define INVALID_ENTITY_INDEX 0
b32 is_valid(u32 entity_index)
{
  b32 result = entity_index == INVALID_ENTITY_INDEX;
  return result;
}

Entity *get_entity(State *state, u32 entity_index)
{
  assert(entity_index != INVALID_ENTITY_INDEX);
  Entity *result = 0;
  Entity *test = state->entities + entity_index;
  if (test->exists)
  {
    result = test;
  }
  return result;
}

void remove_entity(State *state, Entity *e)
{
  u32 last_index = state->entities_count - 1;
  Entity *last_entity = state->entities + last_index;
  if (e != last_entity)
  {
    *e = *last_entity;
  }
  
  state->entities_count--;
}

Entity *add_asteroid(State *state, v2 p, f32 scale)
{
  state->asteroid_count++;
  
  RandomSequence *s = &state->seed;
  Entity *e = add_entity(state, EntityType_ASTEROID);
  
  ShapePolygon *poly = &e->shape;
  *poly = generate_random_convex_polygon(&state->seed, 
                                         random_range_i32(&state->seed, 4, 16), 1.0f);
  
  e->asteroid.scale = scale;
  e->angular_velocity = random_range(s, -0.9f, 0.9f)/scale;
  
  v2 random_v = v2(random_bilateral(s), random_bilateral(s));
  e->velocity = random_v*4/scale;
  e->t.p = p;
  e->t.scale = v2(scale, scale);
  
  return e;
}

Entity *add_bullet(State *state, v2 p, v2 velocity, f32 angle)
{
  Entity *e = add_entity(state, EntityType_BULLET);
  
  ShapePolygon *poly = &e->shape;
  poly->count = 4;
  poly->vertices[0] = v2(-0.2f, -0.2f);
  poly->vertices[1] = v2(-0.2f, 0.2f);
  poly->vertices[2] = v2(0.2f, 0.2f);
  poly->vertices[3] = v2(0.2f, -0.2f);
  
  e->t.p = p;
  e->t.angle = angle;
  e->t.scale = v2(0.8f, 0.5f);
  e->velocity = velocity;
  e->bullet.lifetime = 2.0f;
  return e;
}

Entity *add_temporary_clone(State *state, Entity *e, v2 p)
{
  Entity *clone = add_entity(state, e->type);
  *clone = *e;
  clone->is_temporary = true;
  clone->t.p = p;
  
  return clone;
}

// NOTE(lvl5): collision stuff
RangeF32 project_polygon_vertices_on_normal(ShapePolygon poly, v2 normal)
{
  RangeF32 result = inverted_range_f32();
  
  for (u32 vertex_index = 0;
       vertex_index < poly.count;
       vertex_index++)
  {
    v2 vertex = poly.vertices[vertex_index];
    f32 proj = dot(vertex, normal);
    if (proj < result.min)
    {
      result.min = proj;
    }
    if (proj > result.max)
    {
      result.max = proj;
    }
  }
  
  return result;
}

b32 intersection_along_normal(ShapePolygon a, ShapePolygon b, v2 normal)
{
  // NOTE(lvl5): calculate projections of every vertex of both shapes on the
  // normal and find min and max of both shapes
  
  RangeF32 a_range = project_polygon_vertices_on_normal(a, normal);
  RangeF32 b_range = project_polygon_vertices_on_normal(b, normal);
  
  b32 intersection_not_found = b_range.min > a_range.max ||
    a_range.min > b_range.max;
  
  if (intersection_not_found)
  {
    return false;
  }
  return true;
}

b32 test_polygon_normals(ShapePolygon a, ShapePolygon b)
{
  for (u32 start_vertex_index = 0;
       start_vertex_index < a.count;
       start_vertex_index++)
  {
    u32 end_vertex_index = start_vertex_index == a.count - 1 ? 0 : start_vertex_index + 1;
    v2 start = a.vertices[start_vertex_index];
    v2 end = a.vertices[end_vertex_index];
    
    // NOTE(lvl5): check normal of every surface
    v2 surface = end - start;
    v2 normal = perp(surface);
    
    b32 intersection_found = intersection_along_normal(a, b, normal);
    if (!intersection_found)
    {
      return false;
    }
  }
  
  return true;
}

b32 polygons_intersect(ShapePolygon a, ShapePolygon b)
{
  
  if (!test_polygon_normals(a, b) ||
      !test_polygon_normals(b, a))
  {
    return false;
  }
  
  return true;
}


Entity *check_collision(State *state, Entity *e, EntityType type)
{
  Entity *result = 0;
  
  for (u32 entitiy_index = 1;
       entitiy_index < state->entities_count;
       entitiy_index++)
  {
    Entity *other = get_entity(state, entitiy_index);
    if (other == e)
    {
      continue;
    }
    if (other->type != type)
    {
      continue;
    }
    
    b32 did_collide = polygons_intersect(transform_polygon(e->shape, e->t), transform_polygon(other->shape, other->t));
    if (did_collide)
    {
      result = other;
      break;
    }
  }
  return result;
}

void add_particles(ParticleSystem *s, u32 count, rect2 area, v2 min_direction, v2 max_direction)
{
  for (u32 particle_index = 0;
       particle_index < count;
       particle_index++)
  {
    if (s->items_count == s->items_capacity)
    {
      break;
    }
    Particle *part = s->items + s->items_count++;
    part->t.p = v2(random_range(&s->seed, area.min.x, area.max.x),
                   random_range(&s->seed, area.min.y, area.max.y));
    part->t.scale = v2(1, 1)*random_range(&s->seed, 0.2f, 0.3f);
    
    v2 direction = normalize(v2(random_range(&s->seed, min_direction.x, max_direction.x),
                                random_range(&s->seed, min_direction.y, max_direction.y)));
    part->d_p = direction*random_range(&s->seed, 0.5f, 5.0f);
    part->d_scale = v2(1, 1)*random_range(&s->seed, -0.2f, -2.5f);
  }
}


void simulate_and_draw_particles(ParticleSystem *s, RenderGroup *group, f32 dt)
{
  for (u32 particle_index = 0;
       particle_index < s->items_count;
       particle_index++)
  {
    Particle *part = s->items + particle_index;
    part->t.p += part->d_p*dt;
    part->t.scale += part->d_scale*dt;
    
    rect2 rect = rect_center_size(v2(), v2(1, 1));
    
    if (part->t.scale.x <= 0)
    {
      Particle *last = s->items + s->items_count - 1;
      *part = *last;
      s->items_count--;
    }
    else
    {
      push_rect(group, rect, part->t, COLOR_RED);
    }
  }
}

void generate_asteroids(State *state)
{
  for (u32 asteroid_index = 0;
       asteroid_index < state->asteroids_per_wave;
       asteroid_index++)
  {
    v2 random_p = v2(-0.5f, 0.5f);//v2(random_bilateral(&state->seed),
    //random_bilateral(&state->seed));
    v2 p = hadamard(random_p, state->game_area_size);
    add_asteroid(state, p, 2.5f);
  }
  
  state->asteroids_per_wave += 2;
}

void draw_entity(RenderGroup *render_group, Entity *e)
{
  switch (e->type)
  {
    case EntityType_ASTEROID:
    case EntityType_PLAYER:
    {
      v4 color = COLOR_WHITE;
      
      Transform t = e->t;
      t.scale = v2(1, 1);
      push_polygon(render_group, e->shape, e->t, color);
    } break;
    case EntityType_BULLET:
    {
      push_rect(render_group, polygon_to_rect2(e->shape), e->t, COLOR_WHITE);
    } break;
  }
}


GAME_UPDATE(game_update)
{
  State *state = (State *)memory->data;
  
  if (!state->initialized)
  {
    *state = {};
    u64 permanent_memory_size = megabytes(16);
    u64 transient_memory_size = memory->size - sizeof(state) -
      permanent_memory_size;
    init(&state->arena, memory->data + sizeof(State), permanent_memory_size);
    init(&state->transient_arena, memory->data + sizeof(State) + permanent_memory_size, transient_memory_size);
    
    
    LocalContext ctx = make_context(get_local_context());
    ctx.allocator = arena_allocator;
    ctx.allocator_data = &state->arena;
    push_context(ctx); {
      state->initialized = true;
      state->seed = make_random_sequence(3153273742);
      state->particle_system.seed = make_random_sequence(54625634);
      state->render_group = {};
      state->render_group.transform.scale = meters_to_screen_space(screen, v2(1, 1));
      
      state->particle_system.items_capacity = 10000;
      state->particle_system.items = alloc_array(Particle,
                                                 state->particle_system.items_capacity);
      
#define SHADER_LOC "shaders/basic.glsl"
      
      String shader_src = platform_read_entire_file(const_string(SHADER_LOC));
      gl_ParseResult shader_sources = gl_parse_glsl(shader_src);
      state->shader = gl_create_shader(shader_sources.vertex, shader_sources.fragment);
      
      state->game_area_size = screen->size / PIXELS_PER_METER;
      
      Entity *zero_entity = add_entity(state, EntityType_NONE);
      
      Entity *player = add_entity(state, EntityType_PLAYER);
      ShapePolygon *poly = &player->shape;
      poly->count = 3;
      poly->vertices[0] = v2(0.4f, 0.0f);
      poly->vertices[1] = v2(-0.4f, 0.3f);
      poly->vertices[2] = v2(-0.4f, -0.3f);
      
      state->asteroids_per_wave = 4;
      generate_asteroids(state);
    }pop_context();
  }
  
  f32 dt = input->delta_time;
  RenderGroup *render_group = &state->render_group;
  
  u64 render_memory_mark = get_mark(&state->transient_arena);
  push_transient_context(state);{
    alloc_render_group_buffer(&state->render_group, screen, megabytes(5));
  }pop_context();
  
  
  simulate_and_draw_particles(&state->particle_system, render_group, dt);
  
  if (state->screenshake_timer >= 0)
  {
    f32 screenshake_angle = random_range(&state->seed, -0.01f, 0.01f);
    render_group->transform.angle = screenshake_angle;
    state->screenshake_timer -= dt;
  }
  
  
  for (u32 entitiy_index = 1;
       entitiy_index < state->entities_count;
       entitiy_index++)
  {
    Entity *e = get_entity(state, entitiy_index);
    if (e)
    {
      if (!e->is_temporary)
      {
        v2 area = state->game_area_size;
        v2 half_area = area*0.5f;
        
        e->t.p += e->velocity*dt;
        e->t.angle += e->angular_velocity*dt;
        
        if (e->t.p.x > half_area.x)
        {
          e->t.p.x -= area.x;
        }
        if (e->t.p.x < -half_area.x)
        {
          e->t.p.x += area.x;
        }
        if (e->t.p.y > half_area.y)
        {
          e->t.p.y -= area.y;
        }
        if (e->t.p.y < -half_area.y)
        {
          e->t.p.y += area.y;
        }
        
        v2 camera_scale = screen_space_to_meters(screen, v2(1, 1));
        rect2 camera_rect = rescale_centered(rect_center_size(-render_group->transform.p, v2(2, 2)), camera_scale);
        push_polygon(render_group, rect2_to_polygon(camera_rect), default_transform(), COLOR_WHITE);
        
        rect2 aabb = polygon_to_aabb(transform_polygon(e->shape, e->t));
        
        b32 right_side = aabb.max.x > camera_rect.max.x;
        b32 top_side = aabb.max.y > camera_rect.max.y;
        b32 left_side = aabb.min.x < camera_rect.min.x;
        b32 bottom_side = aabb.min.y < camera_rect.min.y;
        
        
        rect2 area_rect = rect_center_size(v2(), area*2);
        
        if (right_side)
          add_temporary_clone(state, e, e->t.p + v2(area_rect.min.x, 0.0f));
        
        if (left_side)
          add_temporary_clone(state, e, e->t.p + v2(area_rect.max.x, 0.0f));
        
        if (bottom_side)
          add_temporary_clone(state, e, e->t.p + v2(0.0f, area_rect.max.y));
        
        if (top_side)
          add_temporary_clone(state, e, e->t.p + v2(0.0f, area_rect.min.y));
        
        if (bottom_side && right_side)
          add_temporary_clone(state, e, e->t.p + v2(area_rect.min.x,
                                                    area_rect.max.y));
        
        if (top_side && left_side)
          add_temporary_clone(state, e, e->t.p + v2(area_rect.max.x,
                                                    area_rect.min.y));
        
        if (top_side && right_side)
          add_temporary_clone(state, e, e->t.p + v2(area_rect.min.x,
                                                    area_rect.min.y));
        
        if (bottom_side && left_side)
          add_temporary_clone(state, e, e->t.p + v2(area_rect.max.x,
                                                    area_rect.max.y));
      }
    }
  }
  
  
  for (u32 entitiy_index = 1;
       entitiy_index < state->entities_count;
       entitiy_index++)
  {
    Entity *e = get_entity(state, entitiy_index);
    if (e)
    {
      if (!e->is_temporary)
      {
        switch (e->type)
        {
          case EntityType_PLAYER:
          {
            if (input->left.is_down)
            {
              e->t.angle += 4.5f*dt;
            }
            if (input->right.is_down)
            {
              e->t.angle -= 4.5f*dt;
            }
            
            f32 MIN_SCALE = 1;
            f32 MAX_SCALE = 1;
            static f32 current_scale = 1;
            
            if (input->up.is_down)
            {
              v2 accel = rotate(v2(9.0f, 0.0f), e->t.angle);
              e->velocity += accel*dt;
              
              f32 particle_spread = random_range(&state->seed, -0.15f, 0.15f);
              v2 particle_direction = rotate(v2(1.0f, particle_spread),
                                             e->t.angle + PI);
              
              v2 particle_p = rotate(v2(-0.5f, 0.0f), e->t.angle) + e->t.p;
              add_particles(&state->particle_system, 5, rect_center_size(particle_p, v2(0, 0)),
                            particle_direction, particle_direction);
              
              f32 dist = MIN_SCALE - current_scale;
              current_scale += dist*0.03f;
            } 
            else
            {
              f32 dist = MAX_SCALE - current_scale;
              current_scale += dist*0.03f;
            }
            render_group->transform.scale = meters_to_screen_space(screen, v2(1, 1))*current_scale;
            
            b32 can_fire = e->player.shot_cooldown <= 0;
            
#if 1
            if (input->space.is_down && can_fire)
            {
              v2 bullet_vel = rotate(v2(15.0f, 0.0f), e->t.angle);
              add_bullet(state, e->t.p, e->velocity + bullet_vel, e->t.angle);
              e->player.shot_cooldown = 0.15f;
            }
#else
            if (input->space.is_down)
            {
              e->velocity = v2();
            }
#endif
            e->player.shot_cooldown -= dt;
            
            if (len_sqr(e->velocity) > sqr(SHIP_SPEED_LIMIT))
            {
              e->velocity = normalize(e->velocity)*SHIP_SPEED_LIMIT;
            }
            
            Entity *other = check_collision(state, e, EntityType_ASTEROID);
            if (other)
            {
              remove_entity(state, e);
              state->initialized = false;
            }
          } break;
          
          case EntityType_ASTEROID:
          {
          } break;
          
          case EntityType_BULLET:
          {
            e->bullet.lifetime -= dt;
            if (e->bullet.lifetime <= 0)
            {
              remove_entity(state, e);
              break;
            }
            
            Entity *other = check_collision(state, e, EntityType_ASTEROID);
            if (other)
            {
              state->screenshake_timer = 0.2f;
              add_particles(&state->particle_system, 100, rect_center_size(e->t.p, v2(0, 0)),
                            v2(-1, -1), v2(1, 1));
              v2 new_p = other->t.p;
              
              f32 new_scale = other->asteroid.scale*0.5f;
              if (new_scale >= 0.5)
              {
                add_asteroid(state, new_p, new_scale);
                add_asteroid(state, new_p, new_scale);
              }
              
              remove_entity(state, e);
              remove_entity(state, other);
              state->asteroid_count--;
            }
            
          } break;
        }
      }
      
      draw_entity(render_group, e);
    }
  }
  
  
  for (u32 entitiy_index = 1;
       entitiy_index < state->entities_count;
       entitiy_index++)
  {
    Entity *e = get_entity(state, entitiy_index);
    if (e->is_temporary)
    {
      remove_entity(state, e);
    }
  }
  
  if (state->asteroid_count == 0)
  {
    generate_asteroids(state);
  }
  
  push_transient_context(state); {
    draw_render_group(render_group, state->shader);
  }pop_context();
  
  set_mark(&state->transient_arena, render_memory_mark);
  
  render_group->transform.angle = 0;
  
  reset_temp_storage();
}
