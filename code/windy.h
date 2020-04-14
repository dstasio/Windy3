#if !defined(WINDY_H)
/* ========================================================================
   $File: $
   $Date: $
   $Revision: $
   $Creator: Davide Stasio $
   $Notice: (C) Copyright 2014 by Davide Stasio. All Rights Reserved. $
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

struct Image_Data
{
    u32   width;
    u32   height;
    u32   size;
    void *data;
};

struct Texture_Data
{
    ID3D11ShaderResourceView *view;
    ID3D11Texture2D          *handle;
};

struct Camera
{
    v3 pos;
    v3 target;
    v3 up;
};

struct Mesh_Data
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
    ID3D11RenderTargetView *render_target_rgb;
    ID3D11DepthStencilView *render_target_depth;
    ID3D11Buffer *matrix_buff;
    ID3D11Buffer *light_buff;

    Mesh_Data environment;
    Mesh_Data player;
    Texture_Data tex_white;
    Texture_Data tex_yellow;

    m4 cam_matrix;
    m4 proj_matrix;

    Camera main_cam;
    r32 cam_radius;
    r32 cam_vtheta;
    r32 cam_htheta;

    Dir_Light sun_;
    Point_Light lamp;
};

struct Memory_Pool
{
    memory_index size;
    memory_index used;
    u8 *base;
};

#define PushStruct(Pool, Type) (Type *)PushSize_((Pool), sizeof(Type))
#define PushArray(Pool, Length, Type) (Type *)PushSize_((Pool), (Length)*sizeof(Type))
inline void *
PushSize_(Memory_Pool *pool, memory_index size)
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
