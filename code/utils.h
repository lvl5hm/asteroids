#ifndef UTILS_H
#define UTILS_H

#define offsetof(s,m) __builtin_offsetof(s,m)

#include <math.h>
#include <float.h>

typedef unsigned __int8 u8;
typedef unsigned __int16 u16;
typedef unsigned __int32 u32;
typedef unsigned __int64 u64;

typedef __int8 i8;
typedef __int16 i16;
typedef __int32 i32;
typedef __int64 i64;
typedef u8 byte;

typedef float f32;
typedef double f64;

typedef __int32 b32;

#define PI 3.14159265359f

#define U8_MIN 0x00
#define U8_MAX 0xFF
#define U16_MIN 0x0000
#define U16_MAX 0xFFFF
#define U32_MIN 0x00000000
#define U32_MAX 0xFFFFFFFF
#define U64_MIN 0x0000000000000000
#define U64_MAX 0xFFFFFFFFFFFFFFFF

#define I8_MAX 0x7F
#define I8_MIN -I8_MAX-1
#define I16_MAX 0x7FFF
#define I16_MIN -I16_MAX-1
#define I32_MAX 0x7FFFFFFF
#define I32_MIN -I32_MAX-1
#define I64_MAX 0x7FFFFFFFFFFFFFFF
#define I64_MIN -I64_MAX-1

#define F32_MAX FLT_MAX
#define F32_MIN -FLT_MAX


#define kilobytes(count) (1024LL*count)
#define megabytes(count) (1024LL*kilobytes(count))
#define gigabytes(count) (1024LL*megabytes(count))
#define terabytes(count) (1024LL*gigabytes(count))


#define break_here() {int do_the_break = 0;}

#define array_count(array) (sizeof(array) / sizeof(*array))

//#define assert(expr) if (!(expr)) {*(int *)0 = 0;}
#define invalid_default_case() default: assert(false); break;

template <typename F>
struct privDefer {
	F f;
	privDefer(F f) : f(f) {}
	~privDefer() { f(); }
};

template <typename F>
privDefer<F> defer_func(F f) {
	return privDefer<F>(f);
}

#define DEFER_1(x, y) x##y
#define DEFER_2(x, y) DEFER_1(x, y)
#define DEFER_3(x) DEFER_2(x, __COUNTER__)
#define defer(code) auto DEFER_3(_defer_) = defer_func([&](){code;})


#ifdef ASTEROIDS_PROD
#define assert(expr)
#if 0
#define assert(expr)  \
if (!(expr))  \
{ \
  char message_buffer[256]; \
  wsprintf(message_buffer, "%s: line %d", __FILE__, __LINE__); \
  MessageBox(global_window, message_buffer, 0, MB_OK); \
}
#endif
#else
#define assert(expr) if (!(expr)) {*(i32 *)0 = 0;}
#endif

#define swap(a, b) {auto tmp = a; a = b; b = tmp;}


#define repeat_times(n) for (u32 it_index = 0; it_index < n; it_index++)


enum AllocatorMode
{
  AllocatorMode_NONE,
  AllocatorMode_ALLOCATE,
  AllocatorMode_FREE,
  AllocatorMode_REALLOC,
};

#define ALLOCATOR(name) byte* name(AllocatorMode mode, u64 size, u64 oldSize, void *oldMemoryPtr, void *allocator_data, u32 align)
typedef ALLOCATOR(Allocator);

byte *alloc(u64 size, u32 align = 32);

struct Arena
{
  byte *memory;
  u64 capacity;
  u64 mark;
};


void init(Arena *arena, void *memory, u64 capacity)
{
  arena->memory = (byte *)memory;
  arena->capacity = capacity;
  arena->mark = 0;
}

u64 get_mark(Arena *arena)
{
  u64 result = arena->mark;
  return result;
}

void set_mark(Arena *arena, u64 mark)
{
  arena->mark = mark;
}


Arena alloc_arena_memory(u64 capacity)
{
  Arena result;
  void *memory = alloc(capacity);
  init(&result, memory, capacity);
  return result;
}

struct LocalContext
{
  Allocator *allocator;
  void *allocator_data;
  
  Arena temp_storage;
};


struct GlobalContext
{
  LocalContext context_stack[32];
  u32 context_count;
};
thread_local GlobalContext __global_context = {};

Arena __default_temp_storage = {};


void push_context(LocalContext ctx)
{
  __global_context.context_stack[__global_context.context_count++] = ctx;
}

void pop_context()
{
  __global_context.context_count--;
}

LocalContext *get_local_context()
{
  return &__global_context.context_stack[__global_context.context_count - 1];
}

ALLOCATOR(arena_allocator)
{
  if (!allocator_data)
  {
    allocator_data = get_local_context()->allocator_data;
  }
  Arena *arena = (Arena *)allocator_data;
  
  byte *result = 0;
  if (mode == AllocatorMode_ALLOCATE)
  {
    assert(arena->mark + size <= arena->capacity);
    result = (u8 *)arena->memory + arena->mark;
    arena->mark += size;
  }
  
  return result;
}


LocalContext make_context(LocalContext *parent = 0)
{
  LocalContext result = {};
  if (parent)
  {
    result = *parent;
  }
  else
  {
    result.allocator = 0;
    result.allocator_data = 0;
    result.temp_storage = __default_temp_storage;
  }
  
  return result;
}

#define alloc_struct(T, ...) (T *)alloc(sizeof(T), __VA_ARGS__) 
#define alloc_array(T, count, ...) (T *)alloc(sizeof(T)*(count), __VA_ARGS__) 
byte *alloc(u64 size, u32 align)
{
  LocalContext *ctx = get_local_context();
  byte *result = ctx->allocator(AllocatorMode_ALLOCATE, size, 0, 0, 0, align);
  return result;
}

struct Foo {};

void copy_memory(void *dst, void *src, u64 size)
{
  for (u64 i = 0; i < size; i++)
  {
    *((u8 *)(dst) + i) = *((u8 *)(src) + i);
  }
}

ALLOCATOR(temp_allocator)
{
  Arena *arena = &get_local_context()->temp_storage;
  byte *result = arena_allocator(mode, size, oldSize, oldMemoryPtr, arena, align);
  assert(result);
  return result;
}


#define temp_alloc_struct(T, ...) (T *)temp_alloc(sizeof(T), __VA_ARGS__) 
#define temp_alloc_array(T, count, ...) \
(T *)temp_alloc(sizeof(T)*(count), __VA_ARGS__) 
void *temp_alloc(u64 size, u32 align = 32)
{
  void *result = temp_allocator(AllocatorMode_ALLOCATE, size, 0, 0, 0, align);
  return result;
}

u64 get_temp_storage_mark()
{
  LocalContext *ctx = get_local_context();
  Arena *temp_storage = &ctx->temp_storage;
  u64 result = get_mark(temp_storage);
  return result;
}

void set_temp_storage_mark(u64 mark)
{
  LocalContext *ctx = get_local_context();
  Arena *temp_storage = &ctx->temp_storage;
  set_mark(temp_storage, mark);
}

void reset_temp_storage()
{
  LocalContext *ctx = get_local_context();
  Arena *temp_storage = &ctx->temp_storage;
  set_mark(temp_storage, 0);
}


// dynamic array

#define DynamicArray(T, name) \
struct name \
{ \
  T *data; \
  u32 count; \
  u32 capacity; \
  Allocator *allocator; \
  T operator[] (u32 index) { \
    return data[index];\
  } \
}; \
void init(name *array, u32 capacity, Allocator *allocator = 0) \
{ \
  if (allocator == 0) { \
    allocator = get_local_context()->allocator; \
  } \
  void *memory = allocator(AllocatorMode_ALLOCATE, capacity*sizeof(T), 0, 0, 0, 32); \
  T *data = (T *)memory; \
  array->data = data; \
  array->count = 0; \
  array->capacity = capacity; \
  array->allocator = allocator; \
} \
T *push(name *array, T thing) \
{ \
  T *result = 0; \
  if (array->count == array->capacity) \
  { \
    u32 new_capacity = array->capacity*2; \
    void *new_data = array->allocator(AllocatorMode_ALLOCATE, \
    new_capacity*sizeof(T), 0, 0, 0, 32); \
    copy_memory(new_data, array->data, array->count*sizeof(T)); \
    array->data = (T *)new_data; \
    array->capacity = new_capacity; \
  } \
  result = array->data + array->count++; \
  *result = thing; \
  return result; \
} \
u32 get_size(name array) \
{ \
  u32 result = sizeof(T)*array.count; \
  return result; \
}



struct String
{
  char *data;
  u32 count;
};

#define const_string(symbols) make_string(symbols, array_count(symbols) - 1)
String make_string(char *data, u32 count)
{
  String result;
  result.data = data;
  result.count = count;
  return result;
}

char *temp_c_string(String str)
{
  char *result = (char *)temp_alloc(str.count + 1);
  copy_memory(result, str.data, str.count);
  result[str.count] = 0;
  
  return result;
}

String substring(String src, u32 start, u32 end)
{
  String result;
  result.data = src.data + start;
  result.count = end - start;
  
  return result;
}

i32 find_last_index(String str, String substr)
{
  for (u32 i = str.count - substr.count - 1; i >= 0; i--)
  {
    u32 srcIndex = i;
    u32 testIndex = 0;
    while (str.data[srcIndex] == substr.data[testIndex])
    {
      srcIndex++;
      testIndex++;
      if (testIndex == substr.count)
      {
        return i;
      }
    }
  }
  return -1;
}

i32 find_index(String str, String substr)
{
  for (u32 i = 0; i < str.count - substr.count; i++)
  {
    u32 srcIndex = i;
    u32 testIndex = 0;
    while (str.data[srcIndex] == substr.data[testIndex])
    {
      srcIndex++;
      testIndex++;
      if (testIndex == substr.count)
      {
        return i;
      }
    }
  }
  return -1;
}

String concat(String a, String b)
{
  String result;
  result.data = (char *)temp_alloc(a.count + b.count);
  result.count = a.count + b.count;
  copy_memory(result.data, a.data, a.count);
  copy_memory(result.data + a.count, b.data, b.count);
  return result;
}

// NOTE(lvl5): random numbers
/*
this is xoroshiro180+
*/
u64 rotl(const u64 x, i32 k) {
  return (x << k) | (x >> (64 - k));
}

struct RandomSequence
{
  u64 seed[2];
};

RandomSequence make_random_sequence(u64 seed)
{
  RandomSequence result;
  result.seed[0] = seed;
  result.seed[1] = seed*seed;
  return result;
}

u64 random_u64(RandomSequence *s) {
  const u64 s0 = s->seed[0];
  u64 s1 = s->seed[1];
  const u64 result = s0 + s1;
  
  s1 ^= s0;
  s->seed[0] = rotl(s0, 24) ^ s1 ^ (s1 << 16); // a, b
  s->seed[1] = rotl(s1, 37); // c
  
  return result;
}


f32 random(RandomSequence *s)
{
  u64 r_u64 = random_u64(s);
  f32 result = (f32)r_u64/(f32)U64_MAX;
  return result;
}

f32 random_bilateral(RandomSequence *s)
{
  f32 r = random(s);
  f32 result = r*2 - 1.0f;
  return result;
}

f32 random_range(RandomSequence *s, f32 min, f32 max)
{
  f32 r = random(s);
  f32 range = max - min;
  f32 result = r*range + min;
  return result;
}

i32 random_range_i32(RandomSequence *s, i32 min, i32 max)
{
  f32 r = random_range(s, (f32)min, (f32)max);
  i32 result = (i32)roundf(r);
  return result;
}

// NOTE(lvl5): maths
f32 sqr(f32 s)
{
  f32 result = s*s;
  return result;
}

f32 sqrt(f32 s)
{
  f32 result = sqrtf(s);
  return result;
}

f32 sin(f32 s)
{
  f32 result = sinf(s);
  return result;
}

f32 cos(f32 s)
{
  f32 result = cosf(s);
  return result;
}

f32 atan(f32 y, f32 x)
{
  f32 result = atan2f(y, x);
  return result;
}

f32 safe_ratio1(f32 a, f32 b)
{
  f32 result = 1;
  if (b != 0)
  {
    result = a/b;
  }
  return result;
}

f32 safe_ratio0(f32 a, f32 b)
{
  f32 result = 0;
  if (b != 0)
  {
    result = a/b;
  }
  return result;
}

// NOTE(lvl5): vectors

struct v2
{
  f32 x, y;
  
  v2() {x = 0; y = 0;};
  v2(f32 _x, f32 _y) {x = _x; y = _y;};
  v2(i32 _x, i32 _y) {x = (f32)_x; y = (f32)_y;};
};

v2 operator+(v2 a, v2 b)
{
  v2 result = v2(a.x + b.x, a.y + b.y);
  return result;
}
v2& operator+=(v2& a, v2 b)
{
  a = a + b;
  return a;
}
v2 operator-(v2 a, v2 b)
{
  v2 result = v2(a.x - b.x, a.y - b.y);
  return result;
}
v2& operator-=(v2& a, v2 b)
{
  a = a - b;
  return a;
}
v2 operator*(v2 a, f32 s)
{
  v2 result = v2(a.x*s, a.y*s);
  return result;
}
v2 operator*(f32 s, v2 a)
{
  v2 result = a*s;
  return result;
}
v2& operator*=(v2& a, f32 s)
{
  a = a*s;
  return a;
}
v2 operator-(v2 a)
{
  v2 result = a*-1;
  return result;
}
v2 operator/(v2 a, f32 s)
{
  v2 result = a*(1/s);
  return result;
}
v2& operator/=(v2& a, f32 s)
{
  a = a/s;
  return a;
}
bool operator==(v2 a, v2 b)
{
  bool result = a.x == b.x && a.y == b.y;
  return result;
}

f32 dot(v2 a, v2 b)
{
  f32 result = a.x*b.x + a.y*b.y;
  return result;
}

v2 hadamard(v2 a, v2 b)
{
  v2 result;
  result.x = a.x*b.x;
  result.y = a.y*b.y;
  return result;
}

v2 rotate(v2 v, f32 a)
{
  v2 result;
  f32 sin_a = sinf(a);
  f32 cos_a = cosf(a);
  
  result.x = cos_a*v.x - sin_a*v.y;
  result.y = sin_a*v.x + cos_a*v.y;
  return result;
}

f32 len_sqr(v2 v)
{
  f32 result = dot(v, v);
  return result;
}

f32 len(v2 v)
{
  f32 result = sqrt(len_sqr(v));
  return result;
}

v2 normalize(v2 v)
{
  v2 result;
  f32 length = len(v);
  result.x = safe_ratio0(v.x, length);
  result.y = safe_ratio0(v.y, length);
  return result;
}

f32 project(v2 a, v2 b)
{
  f32 result = dot(a, b) / len(b);
  return result;
}

v2 perp(v2 v)
{
  v2 result = v2(-v.y, v.x);
  return result;
}

f32 get_angle(v2 v)
{
  f32 result = atan(v.y, v.x);
  return result;
}

// NOTE(lvl5): v3

struct v3
{
  union
  {
    struct
    {
      f32 x, y, z;
    };
    struct
    {
      f32 r, g, b;
    };
    struct
    {
      v2 xy;
      f32 z;
    };
  };
  
  v3() {x = 0; y = 0; z = 0;}
  v3(f32 _x, f32 _y, f32 _z) {x = _x; y = _y; z = _z;}
  v3(i32 _x, i32 _y, i32 _z) {x = (f32)_x; y = (f32)_y; z = (f32)_z;}
  v3(v2 _xy, f32 _z = 0) {xy = _xy; z = _z;}
};

v3 operator+(v3 a, v3 b)
{
  v3 result = v3(a.x + b.x, a.y + b.y, a.z + b.z);
  return result;
}
v3& operator+=(v3& a, v3 b)
{
  a = a + b;
  return a;
}
v3 operator-(v3 a, v3 b)
{
  v3 result = v3(a.x - b.x, a.y - b.y, a.z - b.z);
  return result;
}
v3& operator-=(v3& a, v3 b)
{
  a = a - b;
  return a;
}
v3 operator*(v3 a, f32 s)
{
  v3 result = v3(a.x*s, a.y*s, a.z*s);
  return result;
}
v3 operator*(f32 s, v3 a)
{
  v3 result = a*s;
  return result;
}
v3& operator*=(v3& a, f32 s)
{
  a = a*s;
  return a;
}
v3 operator-(v3 a)
{
  v3 result = a*-1;
  return result;
}
v3 operator/(v3 a, f32 s)
{
  v3 result = a*(1/s);
  return result;
}
v3& operator/=(v3& a, f32 s)
{
  a = a/s;
  return a;
}
bool operator==(v3 a, v3 b)
{
  bool result = a.x == b.x && a.y == b.y && a.z == b.z;
  return result;
}

f32 dot(v3 a, v3 b)
{
  f32 result = a.x*b.x + a.y*b.y + a.z*b.z;
  return result;
}

v3 hadamard(v3 a, v3 b)
{
  v3 result;
  result.x = a.x*b.x;
  result.y = a.y*b.y;
  result.z = a.z*b.z;
  return result;
}

f32 len_sqr(v3 v)
{
  f32 result = dot(v, v);
  return result;
}

f32 len(v3 v)
{
  f32 result = sqrt(len_sqr(v));
  return result;
}

v3 normalize(v3 v)
{
  v3 result;
  f32 length = len(v);
  result.x = safe_ratio0(v.x, length);
  result.y = safe_ratio0(v.y, length);
  result.z = safe_ratio0(v.z, length);
  return result;
}

f32 project(v3 a, v3 b)
{
  f32 result = dot(a, b) / len(b);
  return result;
}

// NOTE(lvl5): v4

struct v4
{
  union
  {
    struct
    {
      v3 xyz;
      f32 w;
    };
    struct
    {
      v3 rgb;
      f32 a;
    };
    struct
    {
      v2 xy;
      f32 z;
      f32 w;
    };
    struct 
    {
      f32 x, y, z, w;
    };
    struct
    {
      f32 r, g, b, a;
    };
  };
  
  v4() {x = 0; y = 0; z = 0; w = 0;}
  v4(f32 _x, f32 _y, f32 _z, f32 _w) {x = _x; y = _y; z = _z; w = _w;}
  v4(i32 _x, i32 _y, i32 _z, i32 _w) 
  {
    x = (f32)_x; y = (f32)_y;
    z = (f32)_z; w = (f32)_w;
  }
  v4(v3 _xyz, f32 _w = 0) {xyz = _xyz; w = _w;}
};

v4 operator+(v4 a, v4 b)
{
  v4 result = v4(a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w);
  return result;
}
v4& operator+=(v4& a, v4 b)
{
  a = a + b;
  return a;
}
v4 operator-(v4 a, v4 b)
{
  v4 result = v4(a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w);
  return result;
}
v4& operator-=(v4& a, v4 b)
{
  a = a - b;
  return a;
}
v4 operator*(v4 a, f32 s)
{
  v4 result = v4(a.x*s, a.y*s, a.z*s, a.w*s);
  return result;
}
v4 operator*(f32 s, v4 a)
{
  v4 result = a*s;
  return result;
}
v4& operator*=(v4& a, f32 s)
{
  a = a*s;
  return a;
}
v4 operator-(v4 a)
{
  v4 result = a*-1;
  return result;
}
v4 operator/(v4 a, f32 s)
{
  v4 result = a*(1/s);
  return result;
}
v4& operator/=(v4& a, f32 s)
{
  a = a/s;
  return a;
}
bool operator==(v4 a, v4 b)
{
  bool result = a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w;
  return result;
}

f32 dot(v4 a, v4 b)
{
  f32 result = a.x*b.x + a.y*b.y + a.z*b.z + a.w*b.w;
  return result;
}

v4 hadamard(v4 a, v4 b)
{
  v4 result;
  result.x = a.x*b.x;
  result.y = a.y*b.y;
  result.z = a.z*b.z;
  result.w = a.w*b.w;
  return result;
}

f32 len_sqr(v4 v)
{
  f32 result = dot(v, v);
  return result;
}

f32 len(v4 v)
{
  f32 result = sqrt(len_sqr(v));
  return result;
}

v4 normalize(v4 v)
{
  v4 result;
  f32 length = len(v);
  result.x = safe_ratio0(v.x, length);
  result.y = safe_ratio0(v.y, length);
  result.z = safe_ratio0(v.z, length);
  result.w = safe_ratio0(v.w, length);
  return result;
}

f32 project(v4 a, v4 b)
{
  f32 result = dot(a, b) / len(b);
  return result;
}


// NOTE(lvl5): rect2
struct rect2
{
  v2 min;
  v2 max;
  
  rect2() {min = v2(); max = v2();}
  rect2(v2 _min, v2 _max) {min = _min; max = _max;}
};

rect2 rect_min_size(v2 min, v2 size)
{
  rect2 result;
  result.min = min;
  result.max = min + size;
  return result;
}

rect2 rect_center_size(v2 center, v2 size)
{
  rect2 result;
  result.min = center - size/2;
  result.max = center + size/2;
  return result;
}

v2 get_size(rect2 r)
{
  v2 result = r.max - r.min;
  return result;
}

v2 get_center(rect2 r)
{
  v2 result = r.min + get_size(r)/2;
  return result;
}

rect2 move(rect2 r, v2 dist)
{
  rect2 result;
  result.min = r.min + dist;
  result.max = r.max + dist;
  return result;
}

rect2 resize_centered(rect2 r, v2 diff)
{
  rect2 result;
  result.min = r.min - diff/2;
  result.max = r.max + diff/2;
  return result;
}

rect2 inverted_infinity_rect2()
{
  rect2 result;
  result.min = v2(F32_MAX, F32_MAX);
  result.max = v2(F32_MIN, F32_MIN);
  return result;
}

rect2 rescale_centered(rect2 r, v2 scale)
{
  rect2 result;
  result.min = hadamard(r.min, scale);
  result.max = hadamard(r.max, scale);
  return result;
}

// matrix2x2

union m2x2
{
  f32 c[2][2];
  f32 l[4];
  struct
  {
    f32 c00;
    f32 c01;
    f32 c10;
    f32 c11;
  };
  m2x2() {c00 = 0; c01 = 0; c10 = 0; c11 = 0;}
  m2x2(f32 _c00, f32 _c01, f32 _c10, f32 _c11) 
  {c00 = _c00; c01 = _c01; c10 = _c10; c11 = _c11;}
};
v2 top_row(m2x2 m)
{
  v2 result = v2(m.c00, m.c01);
  return result;
}
v2 bottom_row(m2x2 m)
{
  v2 result = v2(m.c10, m.c11);
  return result;
}
v2 left_col(m2x2 m)
{
  v2 result = v2(m.c00, m.c10);
  return result;
}
v2 right_col(m2x2 m)
{
  v2 result = v2(m.c10, m.c11);
  return result;
}
f32 det(m2x2 m)
{
  f32 result = m.c00*m.c11 + m.c01*m.c10;
  return result;
}
m2x2 transpose(m2x2 m)
{
  m2x2 result;
  result.c00 = m.c01;
  result.c01 = m.c11;
  result.c10 = m.c00;
  result.c11 = m.c10;
  return result;
}
m2x2 operator+(m2x2 a, m2x2 b)
{
  m2x2 result;
  result.c00 = a.c00 + b.c00;
  result.c01 = a.c01 + b.c01;
  result.c10 = a.c10 + b.c10;
  result.c11 = a.c11 + b.c11;
  return result;
}
m2x2 operator*(m2x2 a, f32 b)
{
  m2x2 result;
  result.c00 = a.c00*b;
  result.c01 = a.c01*b;
  result.c10 = a.c10*b;
  result.c11 = a.c11*b;
  return result;
}
m2x2 operator*(f32 a, m2x2 b)
{
  m2x2 result = b*a;
  return result;
}
v2 operator*(m2x2 a, m2x2 b)
{
  v2 result;
  result.x = dot(top_row(a), left_col(b));
  result.y = dot(bottom_row(a), right_col(b));
  return result;
}


// range??
struct RangeF32
{
  f32 min;
  f32 max;
};

RangeF32 range_f32(f32 min, f32 max)
{
  RangeF32 result;
  result.min = min;
  result.max = max;
  return result;
}

RangeF32 inverted_range_f32()
{
  RangeF32 result = range_f32(F32_MAX, F32_MIN);
  return result;
}

#endif