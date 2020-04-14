#if !defined(WINDY_PLATFORM_H)
/* ========================================================================
   $File: $
   $Date: $
   $Revision: $
   $Creator: Davide Stasio $
   $Notice: (C) Copyright 2014 by Davide Stasio. All Rights Reserved. $
   ======================================================================== */
#include "windy_types.h"

struct Input_File
{
    u8 *data;
    u64 size;
};

#define PLATFORM_READ_FILE(name) Input_File name(char *Path)
typedef PLATFORM_READ_FILE(platform_read_file);

struct Input_Keyboard
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

struct Input
{
    Input_Keyboard pressed;
    Input_Keyboard held;

    v2i dmouse;
    i16 dwheel;
};

typedef struct Game_Memory
{
    b32 is_initialized;

    u64 storage_size;
    void *storage;

    Input_File vertex_shader_file;

    platform_read_file *read_file;
} game_memory;

#define GAME_UPDATE_AND_RENDER(name) void name(Input *input, r32 dtime, ID3D11Device *device, ID3D11DeviceContext *context, ID3D11Texture2D *rendering_backbuffer, Input_File VertexBytes, Game_Memory *memory)
typedef GAME_UPDATE_AND_RENDER(game_update_and_render);

#define WINDY_PLATFORM_H
#endif
