#if !defined(WIN32_LAYER_H)
/* ========================================================================
   $File: $
   $Date: $
   $Revision: $
   $Creator: Casey Muratori $
   $Notice: (C) Copyright 2014 by Molly Rocket, Inc. All Rights Reserved. $
   ======================================================================== */

struct win32_game_code
{
    HMODULE DLL;
    //char *DLLPath;

    FILETIME WriteTime;
};

#define WIN32_LAYER_H
#endif
