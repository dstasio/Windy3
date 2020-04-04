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

struct image_data
{
    u32   Size;
    u32   Width;
    u32   Height;
    void *Bytes;
};

struct texture
{
    ID3D11Texture2D *Handle;
    ID3D11ShaderResourceView *Resource;
};

struct camera
{
    v3 pos;
    v3 target;
    v3 up;
};

struct game_state
{
    ID3D11RenderTargetView *render_target_rgb;
    ID3D11DepthStencilView *render_target_depth;
    ID3D11Buffer *matrix_buff;
    ID3D11Buffer *vertex_buff;
    ID3D11Buffer * index_buff;

    m4 CameraMatrix;
    m4 ProjectionMatrix;
    r32 theta;

    camera main_cam;
    r32 cam_radius;
    r32 cam_vtheta;
    r32 cam_htheta;
};

struct memory_pool
{
    memory_index Size;
    memory_index Used;
    u8 *Base;
};

#define PushStruct(Pool, Type) (Type *)PushSize_((Pool), sizeof(Type))
#define PushArray(Pool, Length, Type) (Type *)PushSize_((Pool), (Length)*sizeof(Type))
inline void *
PushSize_(memory_pool *Pool, memory_index Size)
{
    Assert((Pool->Used + Size) < Pool->Size);

    void *Result = Pool->Base + Pool->Used;
    Pool->Used += Size;
    return(Result);
}

#define WINDY_H
#endif
