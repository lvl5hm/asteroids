#ifndef RENDERER_H
#define RENDERER_H

#include "platform.h"
#include "opengl.h"


struct Polygon 
{
  v2 vertices[16];
  u32 count;
};

struct Buffer
{
  byte *data;
  u32 size;
  u32 capacity;
};

struct Transform
{
  v2 p;
  v2 scale;
  f32 angle;
};

struct RenderGroup
{
  Buffer buffer;
  Transform transform;
  GameScreen *screen;
};

enum RenderEntryType
{
  RenderEntryType_NONE,
  RenderEntryType_Polygon,
  RenderEntryType_Rect
};

struct RenderEntryPolygon
{
  Polygon shape;
  v4 color;
};

struct RenderEntryRect
{
  Polygon shape;
  v4 color;
};



#define PIXELS_PER_METER 40


v2 screen_space_to_meters(GameScreen *screen, v2 p)
{
  v2 result = hadamard(p, screen->size)/2.0f/PIXELS_PER_METER;
  return result;
}

v2 meters_to_screen_space(GameScreen *screen, v2 p)
{
  v2 pixel_size = p*PIXELS_PER_METER;
  v2 result;
  result.x = pixel_size.x/screen->size.x*2.0f;
  result.y = pixel_size.y/screen->size.y*2.0f;
  
  return result;
}

void alloc_render_group_buffer(RenderGroup *group, GameScreen *screen, u32 capacity)
{
  group->screen = screen;
  group->buffer.capacity = capacity;
  group->buffer.data = alloc(capacity);
  group->buffer.size = 0;
}
v2 transform_vector(v2 v, Transform t)
{
  v2 result;
  result = v;
  result = hadamard(result, t.scale);
  result = rotate(result, t.angle);
  result += t.p;
  return result;
}

Transform default_transform()
{
  Transform result;
  result.p = v2();
  result.angle = 0;
  result.scale = v2(1, 1);
  return result;
}

Transform translate(Transform t, v2 p)
{
  Transform result = t;
  result.p += p;
  return result;
}

Polygon transform_polygon(Polygon s, Transform t)
{
  Polygon result;
  result.count = s.count;
  
  for (u32 vertex_index = 0;
       vertex_index < s.count;
       vertex_index++)
  {
    v2 v = s.vertices[vertex_index];
    result.vertices[vertex_index] = transform_vector(v, t);
  }
  
  return result;
}

#define push_buffer(buffer, T) (T *)push_buffer_(buffer, sizeof(T))
void *push_buffer_(Buffer *buffer, u32 size)
{
  assert(buffer->size + size <= buffer->capacity);
  void *result = (byte *)buffer->data + buffer->size;
  buffer->size += size;
  return result;
}

#define pop_buffer(buffer, T) (T *)pop_buffer_(buffer, sizeof(T))
void *pop_buffer_(Buffer *buffer, u32 size)
{
  assert(buffer->size >= size);
  u32 prev_size = buffer->size;
  buffer->size -= size;
  
  void *result = (byte *)buffer->data + buffer->size;
  return result;
}

#define push_render_entry(group, T) (RenderEntry##T *)push_render_entry_(group, RenderEntryType_##T, sizeof(RenderEntry##T))
void *push_render_entry_(RenderGroup *group, RenderEntryType type, u32 size)
{
  void *entry = push_buffer_(&group->buffer, size);
  RenderEntryType *type_memory = push_buffer(&group->buffer, RenderEntryType);
  *type_memory = type;
  
  return entry;
}


void push_polygon(RenderGroup *group, Polygon polygon, Transform t, v4 color)
{
  RenderEntryPolygon *entry = push_render_entry(group, Polygon);
  entry->shape = transform_polygon(transform_polygon(polygon, t), 
                                   group->transform);
  entry->color = color;
}

rect2 polygon_to_rect2(Polygon p)
{
  assert(p.count == 4);
  rect2 result;
  result.min = p.vertices[0];
  result.max = p.vertices[2];
  
  return result;
}

Polygon rect2_to_polygon(rect2 r)
{
  Polygon result;
  result.count = 4;
  result.vertices[0] = v2(r.min.x, r.min.y);
  result.vertices[1] = v2(r.min.x, r.max.y);
  result.vertices[2] = v2(r.max.x, r.max.y);
  result.vertices[3] = v2(r.max.x, r.min.y);
  
  return result;
}

void push_rect(RenderGroup *group, rect2 rect, Transform t, v4 color)
{
  RenderEntryRect *entry = push_render_entry(group, Rect);
  
  Polygon shape = rect2_to_polygon(rect);
  entry->shape = transform_polygon(transform_polygon(shape, t), 
                                   group->transform);
  entry->color = color;
}


struct VertexInfo
{
  v2 p;
  v4 color;
};


void draw_render_group(RenderGroup *group, u32 shader)
{
  VertexInfo *lines_vertex_infos = 0;
  sb_reserve(lines_vertex_infos, 200);
  
  u32 *lines_indices = 0;
  sb_reserve(lines_indices, 400);
  
  
  VertexInfo *rect_vertex_infos = 0;
  sb_reserve(rect_vertex_infos, 300);
  
  u32 *rect_indices = 0;
  sb_reserve(rect_indices, 1200);
  
  Buffer *buffer = &group->buffer;
  
  while (buffer->size)
  {
    RenderEntryType *type = pop_buffer(buffer, RenderEntryType);
    switch (*type)
    {
      case RenderEntryType_Rect:
      {
        RenderEntryRect *entry =  pop_buffer(buffer, RenderEntryRect);
        
        u32 start_index = sb_count(rect_vertex_infos);
        u32 vertex_index = 0;
        while(vertex_index < entry->shape.count)
        {
          repeat_times(4)
          {
            VertexInfo info;
            info.p = entry->shape.vertices[vertex_index++];
            info.color = entry->color;
            sb_push(rect_vertex_infos, info);
          }
          
          sb_push(rect_indices, start_index+0);
          sb_push(rect_indices, start_index+1);
          sb_push(rect_indices, start_index+2);
          sb_push(rect_indices, start_index+2);
          sb_push(rect_indices, start_index+3);
          sb_push(rect_indices, start_index+0);
        }
        
      } break;
      
      case RenderEntryType_Polygon:
      {
        RenderEntryPolygon *entry = pop_buffer(buffer, RenderEntryPolygon);
        
        u32 start_index = sb_count(lines_vertex_infos);
        for (u32 vertex_index = 0;
             vertex_index < entry->shape.count;
             vertex_index++)
        {
          VertexInfo info;
          info.p = entry->shape.vertices[vertex_index];
          info.color = entry->color;
          sb_push(lines_vertex_infos, info);
          
          u32 current_index = vertex_index + start_index;
          u32 next_vertex_index = vertex_index + 1;
          if (next_vertex_index == entry->shape.count)
          {
            next_vertex_index = 0;
          }
          u32 next_index = next_vertex_index + start_index;
          
          sb_push(lines_indices, current_index);
          sb_push(lines_indices, next_index);
        }
      } break;
      
      invalid_default_case();
    }
  }
  assert(buffer->size == 0);
  
  
  glClearColor(0, 0, 0, 1);
  glClear(GL_COLOR_BUFFER_BIT);
  
  // NOTE(lvl5): setting up buffers
  glUseProgram(shader);
  
  
  u32 vao;
  glGenVertexArrays(1, &vao);
  glBindVertexArray(vao);
  
  u32 vbo;
  glGenBuffers(1, &vbo);
  
  u32 ibo;
  glGenBuffers(1, &ibo);
  
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(VertexInfo), (void *)offsetof(VertexInfo, p));
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(VertexInfo), (void *)offsetof(VertexInfo, color));
  
  
  // NOTE(lvl5): drawing lines
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, sb_size(lines_vertex_infos), lines_vertex_infos, GL_STATIC_DRAW);
  
  
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sb_size(lines_indices), lines_indices, GL_STATIC_DRAW);
  
  glDrawElements(GL_LINES, sb_count(lines_indices), GL_UNSIGNED_INT, 0);
  
  
  // NOTE(lvl5): drawing rects
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, sb_size(rect_vertex_infos), rect_vertex_infos, GL_STATIC_DRAW);
  
  
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sb_size(rect_indices), rect_indices, GL_STATIC_DRAW);
  
  glDrawElements(GL_TRIANGLES, sb_count(rect_indices), GL_UNSIGNED_INT, 0);
  
  glDeleteVertexArrays(1, &vao);
  glDeleteBuffers(1, &vbo);
  glDeleteBuffers(1, &ibo);
}


#endif