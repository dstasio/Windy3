#if !defined(WINDY_H)
/* ========================================================================
   $File: $
   $Date: $
   $Revision: $
   $Creator: Davide Stasio $
   $Notice: (C) Copyright 2020 by Davide Stasio. All Rights Reserved. $
   ======================================================================== */
#include "windy_platform.h"

#define WIDTH 1024
#define HEIGHT 720

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
 
struct Game_State
{
    Platform_Shader  *phong_shader;
    Platform_Shader  *font_shader;

    Mesh     environment;
    Mesh     player;
    Platform_Texture  tex_white;
    Platform_Texture  tex_yellow;
    Platform_Font     inconsolata;

    m4      cam_matrix;
    m4      proj_matrix;

    Camera  main_cam;
    r32     cam_radius;
    r32     cam_vtheta;
    r32     cam_htheta;

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
