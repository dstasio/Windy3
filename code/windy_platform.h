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

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t  i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;
typedef i32 b32;

typedef size_t memory_index;

typedef float  r32;
typedef double r64;

#if WINDY_INTERNAL
#define Assert(x) (if(!(x)) *((int *)0) = 0;)
#else
#define Assert(x)
#endif

struct file
{
    u8 *Data;
    u64 Size;
};

#define PLATFORM_READ_FILE(name) file name(char *Path)
typedef PLATFORM_READ_FILE(platform_read_file);
 
typedef struct game_memory
{
    b32 IsInitialized;

    u64 StorageSize;
    void *Storage;

    platform_read_file *ReadFile;
} game_memory;

#define GAME_UPDATE_AND_RENDER(name) void name(ID3D11Device *Device, ID3D11DeviceContext *Context, ID3D11RenderTargetView *View, game_memory *Memory)
typedef GAME_UPDATE_AND_RENDER(game_update_and_render);

#define WINDY_PLATFORM_H
#endif
