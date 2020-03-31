#if !defined(WINDY_INTRINSICS_H)
/* ========================================================================
   $File: $
   $Date: $
   $Revision: $
   $Creator: Davide Stasio $
   $Notice: (C) Copyright 2020 by Davide Stasio. All Rights Reserved. $
   ======================================================================== */

#include "windy_types.h"
#include <math.h>

//
// all trigonometric functions are in radians
//

inline r32
Sin(r32 rad)
{
    r32 result = (r32)sin(rad);
    return result;
}

inline r32
Cos(r32 rad)
{
    r32 result = (r32)cos(rad);
    return result;
}

inline r32
Tan(r32 rad)
{
    r32 result = (r32)tan(rad);
    return result;
}

inline r32
Sqrt(r32 a)
{
    r32 result = (r32)sqrt(a);
    return result;
}

#define WINDY_INTRINSICS_H
#endif
