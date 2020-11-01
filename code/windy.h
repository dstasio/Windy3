#if !defined(WINDY_H)
/* ========================================================================
   $File: $
   $Date: $
   $Revision: $
   $Creator: Davide Stasio $
   $Notice: (C) Copyright 2020 by Davide Stasio. All Rights Reserved. $
   ======================================================================== */
#include "windy_platform.h"

#if WINDY_DEBUG
#define Assert(expr) if(!(expr)) {*(int *)0 = 0;}
#else
#define Assert(expr)
#endif
#include "windy_math.h"

#define byte_offset(base, offset) ((u8*)(base) + (offset))
inline u16 truncate_to_u16(u32 v) {assert(v <= 0xFFFF); return (u16)v; };

struct Camera
{
    v3 pos;
    v3 target;
    v3 up;

    r32 fov;
    r32 min_z;
    r32 max_z;

    b32 is_ortho;
    r32 ortho_scale;

    r32 _radius; // Variables used for third person camera
    r32 _pitch;
    r32 _yaw;

    v3 _pivot;   // Used for editor camera
};

struct Mesh
{
    Platform_Mesh_Buffers buffers;

    m4 transform;
    v3 p;
    v3 dp;
    v3 ddp;
};

struct Point_Light
{
    v3 color;
    v3 p;
};

struct Dir_Light
{
    v3 color;
    v3 dir;
};

#define MAX_LEVEL_OBJECTS 20
// @todo: Mesh struct should keep all the info needed for rendering
struct Level
{
    Mesh objects[MAX_LEVEL_OBJECTS];
    u32  n_objects;

    Mesh &get(u32 index)  {return objects[index];}
    Mesh &last() {return this->get(n_objects);};
};
 
struct Game_State
{
    Platform_Shader  *phong_shader;
    Platform_Shader  *font_shader;

    Level current_level;
    Mesh *selected;
    Mesh *env;
    Mesh *player;

    Platform_Texture  tex_white;
    Platform_Texture  tex_yellow;
    Platform_Font     inconsolata;

    m4 cam_matrix;
    m4 proj_matrix;

    Camera game_camera;
    Camera editor_camera;

    Dir_Light    sun_;
    Point_Light  lamp;
};

struct Memory_Pool
{
    memory_index size;
    memory_index used;
    u8 *base;
};

#define push_struct(pool, type) (type *)push_size_((pool), sizeof(type))
#define push_array(pool, length, type) (type *)push_size_((pool), (length)*sizeof(type))
inline void *
push_size_(Memory_Pool *pool, memory_index size)
{
    Assert((pool->used + size) < pool->size);

    void *result = pool->base + pool->used;
    pool->used += size;
    return(result);
}

#define WINDY_H
#endif
