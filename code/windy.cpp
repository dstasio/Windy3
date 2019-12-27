/* ========================================================================
   $File: $
   $Date: $
   $Revision: $
   $Creator: Casey Muratori $
   $Notice: (C) Copyright 2014 by Molly Rocket, Inc. All Rights Reserved. $
   ======================================================================== */
#include <d3d11.h>
#include "windy_platform.h"

GAME_UPDATE_AND_RENDER(WindyUpdateAndRender)
{
    real32 ClearColor[] = {1.f, 0.f, 0.f, 1.f};
    Context->ClearRenderTargetView(View, ClearColor);
}
