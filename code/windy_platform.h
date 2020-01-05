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

#define Kilobytes(k) (1024LL*(k))
#define Megabytes(m) (1024LL*Kilobytes(m))
#define Gigabytes(g) (1024LL*Megabytes(g))
#define Terabytes(t) (1024LL*Gigabytes(t))

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

struct file
{
    u8 *Data;
    u64 Size;
};

#define PLATFORM_READ_FILE(name) file name(char *Path)
typedef PLATFORM_READ_FILE(platform_read_file);

struct input_key
{
    u32 IsHeld;
    u32 WasPressed;
};

struct input_keyboard
{
    u32 Up;
    u32 Down;
    u32 Left;
    u32 Right;
    u32 Esc;
};

struct input
{
    input_keyboard Pressed;
    input_keyboard Held;
};
 
typedef struct game_memory
{
    b32 IsInitialized;

    u64 StorageSize;
    void *Storage;

    file VertexShaderBytes;

    platform_read_file *ReadFile;
} game_memory;

#define GAME_UPDATE_AND_RENDER(name) void name(input *Input, ID3D11Device *Device, ID3D11DeviceContext *Context, ID3D11RenderTargetView *View, file VertexBytes, game_memory *Memory)
typedef GAME_UPDATE_AND_RENDER(game_update_and_render);

#define WINDY_PLATFORM_H
#endif
