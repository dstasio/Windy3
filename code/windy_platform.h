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
    char *path;
    u8   *data;
    u64   size;
    u64   write_time;  // @todo: this should be useless in release build
};

#define PLATFORM_READ_FILE(name) Input_File name(char *Path)
typedef PLATFORM_READ_FILE(Platform_Read_File);
#define PLATFORM_RELOAD_CHANGED_FILE(name) b32 name(Input_File *file)
typedef PLATFORM_RELOAD_CHANGED_FILE(Platform_Reload_Changed_File);

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

    Platform_Read_File           *read_file;
    Platform_Reload_Changed_File *reload_if_changed;
} game_memory;

#define GAME_UPDATE_AND_RENDER(name) void name(Input *input, r32 dtime, ID3D11Device *device, ID3D11DeviceContext *context, ID3D11Texture2D *rendering_backbuffer, Game_Memory *memory)
typedef GAME_UPDATE_AND_RENDER(Game_Update_And_Render);

#define WINDY_PLATFORM_H
#endif
