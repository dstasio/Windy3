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
    u8 *data;
    u64 size;
};

#define PLATFORM_READ_FILE(name) file name(char *Path)
typedef PLATFORM_READ_FILE(platform_read_file);

struct input_keyboard
{
    u32 up;
    u32 down;
    u32 left;
    u32 right;

    u32 w;
    u32 a;
    u32 s;
    u32 d;

    u32 f;

    u32 space;
    u32 shift;
    u32 ctrl;
    u32 alt;
    u32 esc;
};

struct input
{
    input_keyboard Pressed;
    input_keyboard Held;

    i16 m_x;
    i16 m_y;
    i32 dm_x;
    i32 dm_y;
};
 
typedef struct game_memory
{
    b32 IsInitialized;

    u64 StorageSize;
    void *Storage;

    file VertexShaderBytes;

    platform_read_file *read_file;
} game_memory;

#define GAME_UPDATE_AND_RENDER(name) void name(input *Input, r32 dtime, ID3D11Device *Device, ID3D11DeviceContext *Context, ID3D11Texture2D *rendering_backbuffer, file VertexBytes, game_memory *Memory)
typedef GAME_UPDATE_AND_RENDER(game_update_and_render);

#define WINDY_PLATFORM_H
#endif
