#if !defined(WINDY_H)
/* ========================================================================
   $File: $
   $Date: $
   $Revision: $
   $Creator: Casey Muratori $
   $Notice: (C) Copyright 2014 by Molly Rocket, Inc. All Rights Reserved. $
   ======================================================================== */
#include "windy_platform.h"

#if WINDY_DEBUG
#define Assert(expr) if(!(expr)) {*(int *)0 = 0;}
#else
#define Assert(expr)
#endif

struct image_texture
{
    u32   Size;
    u32   Width;
    u32   Height;
    void *Bytes;
};

struct game_state
{
    ID3D11Buffer *ConstantBuffer;
    ID3D11Buffer *VertexBuffer;
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
