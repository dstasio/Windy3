#if !defined(WINDY_MATH_H)
/* ========================================================================
   $File: $
   $Date: $
   $Revision: $
   $Creator: Davide Stasio $
   $Notice: (C) Copyright 2020 by Davide Stasio. All Rights Reserved. $
   ======================================================================== */
#include "windy_intrinsics.h"

#define PI 3.141592653589793238462643383279f
#define DegToRad (PI/180.f)
#define RadToDeg (180.f/PI)
#define COL_X 0
#define COL_Y 1
#define COL_Z 2
#define COL_W 3

struct v3
{
    union
    {
        struct { r32 x, y, z; };
        r32    row[3];
    };
};

v3 operator-(v3 a)
{
    v3 result = {-a.x, -a.y, -a.z};
    return result;
}

v3 operator*(v3 a, r32 b)
{
    v3 result = {a.x*b, a.y*b, a.z*b};
    return result;
}

v3 operator/(v3 a, r32 b)
{
    v3 result = {a.x/b, a.y/b, a.z/b};
    return result;
}

v3 operator-(v3 a, v3 b)
{
    v3 result = {a.x-b.x, a.y-b.y, a.z-b.z};
    return result;
}

v3 operator-(v3 a, r32 b)
{
    v3 result = {a.x-b, a.y-b, a.z-b};
    return result;
}

inline r32
Dot(v3 a, v3 b)
{
    r32 result = a.x*b.x + a.y*b.y + a.z*b.z;
    return result;
}

inline v3
Cross(v3 a, v3 b)
{
    v3 result = {
        a.y*b.z - a.z*b.y,
        a.z*b.x - a.x*b.z,
        a.x*b.y - a.y*b.x
    };
    return result;
}

inline r32
Length_Sq(v3 a)
{
    r32 result = Dot(a, a);
    return result;
}

inline r32
Length(v3 a)
{
    r32 result = Length_Sq(a);
    if(result != 1)
    {
        result = Sqrt(result);
    }
    return result;
}

inline v3
Normalize(v3 a)
{
    v3 result = a / Length(a);
    return result;
}

struct v4
{
    union
    {
        struct { r32 x, y, z, w; };
        r32    row[4];
    };
};

inline r32
Dot(v4 a, v4 b)
{
    r32 result = a.x*b.x + a.y*b.y + a.z*b.z + a.w*b.w;
    return result;
}

struct m4
{
    union
    {
        r32 m[4][4];
        v4  col [4];
    };
};

inline m4
Transpose(m4 a)
{
    m4 matrix = {
        a.m[0][0], a.m[1][0], a.m[2][0], a.m[3][0],
        a.m[0][1], a.m[1][1], a.m[2][1], a.m[3][1],
        a.m[0][2], a.m[1][2], a.m[2][2], a.m[3][2],
        a.m[0][3], a.m[1][3], a.m[2][3], a.m[3][3]
    };
    return matrix;
}

m4 operator*(m4 a, m4 b)
{
    m4 matrix = {};
    a = Transpose(a);

    for(u32 row_index = 0;
        row_index < 4;
        ++row_index)
    {
        for(u32 col_index = 0;
            col_index < 4;
            ++col_index)
        {
            matrix.m[col_index][row_index] = Dot(a.col[row_index], b.col[col_index]);
        }
    }
    return matrix;
}

inline m4
Identity_m4()
{
    m4 matrix = {
        1.f, 0.f, 0.f, 0.f,
        0.f, 1.f, 0.f, 0.f,
        0.f, 0.f, 1.f, 0.f,
        0.f, 0.f, 0.f, 1.f
    };
    return matrix;
}

inline m4
Flip_m4(i32 first_comp, i32 second_comp, i32 third_comp, i32 fourth_comp)
{
    Assert((first_comp >= 0) && (first_comp < 4));
    Assert((second_comp >= 0) && (second_comp < 4));
    Assert((third_comp >= 0) && (third_comp < 4));
    Assert((fourth_comp >= 0) && (fourth_comp < 4));
    m4 matrix = {};
    matrix.m[0][first_comp] = 1.f;
    matrix.m[1][second_comp] = 1.f;
    matrix.m[2][third_comp] = 1.f;
    matrix.m[3][fourth_comp] = 1.f;

    return matrix;
}

inline m4
Scale_m4(v3 s)
{
    m4 matrix = {
        s.x, 0.f, 0.f, 0.f,
        0.f, s.y, 0.f, 0.f,
        0.f, 0.f, s.z, 0.f,
        0.f, 0.f, 0.f, 1.f
    };
    return matrix;
}
inline m4
Scale_m4(r32 s) { return Scale_m4({s, s, s}); }

inline m4
Translation_m4(v3 t)
{
    m4 matrix = {
        1.f, 0.f, 0.f, 0.f,
        0.f, 1.f, 0.f, 0.f,
        0.f, 0.f, 1.f, 0.f,
        t.x, t.y, t.z, 1.f
    };
    return matrix;
}
inline m4
Translation_m4(r32 t) { return Translation_m4({t, t, t}); }

inline m4
Pitch_m4(r32 delta) // Around X axis
{
    r32 sin = Sin(delta);
    r32 cos = Cos(delta);
    m4 matrix = {
        1.f,  0.f, 0.f, 0.f,
        0.f,  cos, sin, 0.f,
        0.f, -sin, cos, 0.f,
        0.f,  0.f, 0.f, 1.f
    };
    return matrix;
}

inline m4
Roll_m4(r32 delta) // Around Y axis
{
    r32 sin = Sin(delta);
    r32 cos = Cos(delta);
    m4 matrix = {
         cos, 0.f, sin, 0.f,
         0.f, 1.f, 0.f, 0.f,
        -sin, 0.f, cos, 0.f,
         0.f, 0.f, 0.f, 1.f
    };
    return matrix;
}

inline m4
Yaw_m4(r32 delta) // Around Z axis
{
    r32 sin = Sin(delta);
    r32 cos = Cos(delta);
    m4 matrix = {
         cos, sin, 0.f, 0.f,
        -sin, cos, 0.f, 0.f,
         0.f, 0.f, 1.f, 0.f,
         0.f, 0.f, 0.f, 1.f
    };
    return matrix;
}

/*
 * Euler angles: (pitch, roll, yaw) in that order
*/
inline m4
Rotation_m4(v3 r)
{
    m4 matrix = Pitch_m4(r.x) * Roll_m4(r.y) * Yaw_m4(r.z);
    return matrix;
}

inline m4
Camera_m4(v3 pos, v3 target, v3 up)
{
    v3 n = Normalize(target - pos);
    v3 v = Normalize(Cross(n, up));
    v3 u = Normalize(Cross(v, n));
    r32 p_v = -Dot(pos, v);
    r32 p_u = -Dot(pos, u);
    r32 p_n = -Dot(pos, n);

    m4 matrix = {
        v.x, u.x, n.x, 0.f,
        v.y, u.y, n.y, 0.f,
        v.z, u.z, n.z, 0.f,
        p_v, p_u, p_n, 1.f
    };

    return matrix;
}

inline m4
Perspective_m4(r32 fov, r32 ar, r32 n, r32 f)
{
    r32 cot = 1.f/Tan(fov/2.f);
    //m4 matrix = {
    //    cot/ar, 0.f,         0.f, 0.f,
    //    0.f,    cot,         0.f, 0.f,
    //    0.f,    0.f, (f+n)/(f-n), 1.f,
    //    0.f,    0.f, 2*f*n/(n-f), 0.f
    //};
    m4 matrix = {
        cot/ar, 0.f,         0.f, 0.f,
        0.f,    cot,         0.f, 0.f,
        0.f,    0.f, -f/(n-f), 1.f,
        0.f,    0.f, n*f/(n-f), 0.f
    };

    return matrix;
}

#define WINDY_MATH_H
#endif
