#if !defined(WIN32_LAYER_H)
/* ========================================================================
   $File: $
   $Date: $
   $Revision: $
   $Creator: Davide Stasio $
   $Notice: (C) Copyright 2014 by Davide Stasio. All Rights Reserved. $
   ======================================================================== */

struct win32_game_code
{
    HMODULE DLL;
    //char *DLLPath;
    FILETIME WriteTime;

    game_update_and_render *GameUpdateAndRender;
};

struct shader
{
    file Bytes;
    FILETIME WriteTime;
};

#define WIN32_LAYER_H
#endif
