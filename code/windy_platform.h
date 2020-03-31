#if !defined(WINDY_PLATFORM_H)
/* ========================================================================
   $File: $
   $Date: $
   $Revision: $
   $Creator: Davide Stasio $
   $Notice: (C) Copyright 2014 by Davide Stasio. All Rights Reserved. $
   ======================================================================== */
#include "windy_types.h"

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

#define GAME_UPDATE_AND_RENDER(name) void name(input *Input, r32 dtime, ID3D11Device *Device, ID3D11DeviceContext *Context, ID3D11RenderTargetView *View, file VertexBytes, game_memory *Memory)
typedef GAME_UPDATE_AND_RENDER(game_update_and_render);

#define WINDY_PLATFORM_H
#endif
