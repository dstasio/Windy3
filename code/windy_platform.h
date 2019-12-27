#if !defined(WINDY_PLATFORM_H)
/* ========================================================================
   $File: $
   $Date: $
   $Revision: $
   $Creator: Casey Muratori $
   $Notice: (C) Copyright 2014 by Molly Rocket, Inc. All Rights Reserved. $
   ======================================================================== */
#include <stdint.h>

#define global static
#define internal static
#define local_persist static

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;
typedef int32 bool32;

typedef size_t memory_index;

typedef float real32;
typedef double real64;

#if WINDY_INTERNAL
#define Assert(x) (if(!(x)) *((int *)0) = 0;)
#else
#define Assert(x)
#endif

#define GAME_UPDATE_AND_RENDER(name) void name(ID3D11DeviceContext *Context, ID3D11RenderTargetView *View)
typedef GAME_UPDATE_AND_RENDER(game_update_and_render);

typedef struct game_memory
{
    bool32 IsInitialized;

    uint64 StorageSize;
    void *Storage;

    game_update_and_render *GameUpdateAndRender;
} game_memory;

#define WINDY_PLATFORM_H
#endif
