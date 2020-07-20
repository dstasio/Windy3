#if !defined(WINDY_H)
/* ========================================================================
   $File: $
   $Date: $
   $Revision: $
   $Creator: Davide Stasio $
   $Notice: (C) Copyright 2014 by Davide Stasio. All Rights Reserved. $
   ======================================================================== */
#include "windy_platform.h"
#define STB_TRUETYPE_IMPLEMENTATION 1
#include "stb_truetype.h"

#define WIDTH 1024
#define HEIGHT 720

#if WINDY_DEBUG
#define Assert(expr) if(!(expr)) {*(int *)0 = 0;}
#else
#define Assert(expr)
#endif
#include "windy_math.h"

struct Texture
{
    ID3D11ShaderResourceView *view;
    ID3D11Texture2D          *handle;

    u32   width;
    u32   height;
    u32   size;
    void *data;
};

#define first_nonwhite_char '!'
#define last_nonwhite_char  '~'
#define n_supported_characters (1+last_nonwhite_char-first_nonwhite_char)
struct Font
{
    stbtt_fontinfo info;
    r32 height, scale;
    Texture chars[n_supported_characters];
};

struct Camera
{
    v3 pos;
    v3 target;
    v3 up;
};

struct Mesh
{
    ID3D11InputLayout *in_layout;
    ID3D11Buffer      *vbuff;
    ID3D11Buffer      *ibuff;

    u16 index_count;
    u8  vert_stride;

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
    Mesh     square;
    Texture  tex_white;
    Texture  tex_yellow;
    Font     inconsolata;

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

#pragma pack(push, 1)
struct Bitmap_Header
{
    u16 Signature;
    u32 FileSize;
    u32 Reserved;
    u32 DataOffset;
    u32 InfoHeaderSize;
    u32 Width;
    u32 Height;
    u16 Planes;
    u16 BitsPerPixel;
    u32 Compression;
    u32 ImageSize;
    u32 XPixelsPerMeter;
    u32 YPixelsPerMeter;
    u32 ColorsUsed;
    u32 ImportantColors;
};

struct Wexp_Header
{
    u16 signature;
    u16 vert_offset;
    u32 indices_offset;
    u32 eof_offset;
};
#pragma pack(pop)

#define WINDY_H
#endif
