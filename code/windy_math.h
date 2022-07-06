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

//
// Scalar ------------------------------------------------------------
//
inline r32 Square(r32 x)
{
    r32 result = x*x;
    return result;
}

inline r32 abs(r32 x)
{
    if (x < 0)  return -x;
    return x;
}

inline bool equal(r32 a, r32 b)
{
    // @todo: proper float comparison here
    constexpr r32 epsilon = 0.005f;

    bool result = abs(a - b) <= epsilon;
    return result;
}

//
// Vector 2 ----------------------------------------------------------
//
inline v2 make_v2(r32 x, r32 y) { return {x, y}; }

v2 operator*(v2 a, r32 b)
{
    v2 result = {a.x*b, a.y*b};
    return result;
}

v2 &operator*=(v2 &a, r32 b)
{
    a = a * b;
    return a;
}

//
// Vector 3 ----------------------------------------------------------
// 

inline v3 make_v3(r32 x, r32 y, r32 z) { return {  x,   y,   z}; }
inline v3 make_v3(v4  a)               { return {a.x, a.y, a.z}; }
inline v3 make_v3(v2  a)               { return {a.x, a.y, 0.f}; }
inline v3 make_v3(r32 a)               { return {  a,   a,   a}; }

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
v3 operator*(r32 a, v3 b)  {return b * a;}

v3 operator/(v3 a, r32 b)
{
    v3 result = {a.x/b, a.y/b, a.z/b};
    return result;
}

v3 operator+(v3 a, v3 b)
{
    v3 result = {a.x+b.x, a.y+b.y, a.z+b.z};
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

v3 &operator*=(v3 &a, r32 b)
{
    a = a * b;
    return a;
}

v3 &operator/=(v3 &a, r32 b)
{
    a = a / b;
    return a;
}

v3 &operator+=(v3 &a, v3 b)
{
    a = a + b;
    return a;
}

v3 &operator-=(v3 &a, v3 b)
{
    a = a - b;
    return a;
}

inline r32
dot(v3 a, v3 b)
{
    r32 result = a.x*b.x + a.y*b.y + a.z*b.z;
    return result;
}

inline v3
cross(v3 a, v3 b)
{
    v3 result = {
        a.y*b.z - a.z*b.y,
        a.z*b.x - a.x*b.z,
        a.x*b.y - a.y*b.x
    };
    return result;
}

inline r32
length_Sq(v3 a)
{
    r32 result = dot(a, a);
    return result;
}

inline r32
length(v3 a)
{
    r32 result = length_Sq(a);
    if(result != 1)
    {
        result = Sqrt(result);
    }
    return result;
}

inline v3
normalize(v3 a)
{
    v3 result = a / length(a);
    return result;
}

inline v3
lerp(v3 a, v3 b, r32 t)
{
    v3 result = a*t + b*(1.f-t);
    return result;
}

//
// Vector 4 ----------------------------------------------------------
// 
inline v4 make_v4(r32 x, r32 y, r32 z, r32 w) { return {  x,   y,   z,   w}; }
inline v4 make_v4(v3  a)                      { return {a.x, a.y, a.z, 1.f}; }
inline v4 make_v4(r32 a)                      { return {  a,   a,   a,   a}; }

inline r32
dot(v4 a, v4 b)
{
    r32 result = a.x*b.x + a.y*b.y + a.z*b.z + a.w*b.w;
    return result;
}

//
// Matrix 4 ----------------------------------------------------------
// Column Major
// @todo: maybe add m4t (matrix4 transposed)?

inline bool equal(m4 a, m4 b)
{
    for (int index = 0; index < 16; ++index) {
        if (!equal(a.arr[index], b.arr[index]))  return false;
    }

    return true;
}

inline m4
identity_m4()
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
transpose(m4 *a)
{
    m4 matrix = {
        a->m[0][0], a->m[1][0], a->m[2][0], a->m[3][0],
        a->m[0][1], a->m[1][1], a->m[2][1], a->m[3][1],
        a->m[0][2], a->m[1][2], a->m[2][2], a->m[3][2],
        a->m[0][3], a->m[1][3], a->m[2][3], a->m[3][3]
    };
    return matrix;
}

v4 operator*(m4 a, v4 b)
{
    v4 result = {};
    a = transpose(&a);  // column-major to row-major

    result.x = dot(a.col[0], b);
    result.y = dot(a.col[1], b);
    result.z = dot(a.col[2], b);
    result.w = dot(a.col[3], b);
    return result;
}

v3 operator*(m4 a, v3 b)
{
    v3 result = make_v3(a*make_v4(b));
    return result;
}

m4 operator*(m4 a, m4 b)
{
    m4 matrix = {};
    a = transpose(&a);

    for(u32 row_index = 0;
        row_index < 4;
        ++row_index)
    {
        for(u32 col_index = 0;
            col_index < 4;
            ++col_index)
        {
            matrix.m[col_index][row_index] = dot(a.col[row_index], b.col[col_index]);
        }
    }
    return matrix;
}

m4 &operator*=(m4 &a, m4 b)
{
    a = a*b;
    return a;
}

inline m4
invert(m4 a)
{
    m4 result = {};

    r64 A2323 = (r64)a.m[2][2] * (r64)a.m[3][3] - (r64)a.m[2][3] * (r64)a.m[3][2];
    r64 A1323 = (r64)a.m[2][1] * (r64)a.m[3][3] - (r64)a.m[2][3] * (r64)a.m[3][1];
    r64 A1223 = (r64)a.m[2][1] * (r64)a.m[3][2] - (r64)a.m[2][2] * (r64)a.m[3][1];
    r64 A0323 = (r64)a.m[2][0] * (r64)a.m[3][3] - (r64)a.m[2][3] * (r64)a.m[3][0];
    r64 A0223 = (r64)a.m[2][0] * (r64)a.m[3][2] - (r64)a.m[2][2] * (r64)a.m[3][0];
    r64 A0123 = (r64)a.m[2][0] * (r64)a.m[3][1] - (r64)a.m[2][1] * (r64)a.m[3][0];
    r64 A2313 = (r64)a.m[1][2] * (r64)a.m[3][3] - (r64)a.m[1][3] * (r64)a.m[3][2];
    r64 A1313 = (r64)a.m[1][1] * (r64)a.m[3][3] - (r64)a.m[1][3] * (r64)a.m[3][1];
    r64 A1213 = (r64)a.m[1][1] * (r64)a.m[3][2] - (r64)a.m[1][2] * (r64)a.m[3][1];
    r64 A2312 = (r64)a.m[1][2] * (r64)a.m[2][3] - (r64)a.m[1][3] * (r64)a.m[2][2];
    r64 A1312 = (r64)a.m[1][1] * (r64)a.m[2][3] - (r64)a.m[1][3] * (r64)a.m[2][1];
    r64 A1212 = (r64)a.m[1][1] * (r64)a.m[2][2] - (r64)a.m[1][2] * (r64)a.m[2][1];
    r64 A0313 = (r64)a.m[1][0] * (r64)a.m[3][3] - (r64)a.m[1][3] * (r64)a.m[3][0];
    r64 A0213 = (r64)a.m[1][0] * (r64)a.m[3][2] - (r64)a.m[1][2] * (r64)a.m[3][0];
    r64 A0312 = (r64)a.m[1][0] * (r64)a.m[2][3] - (r64)a.m[1][3] * (r64)a.m[2][0];
    r64 A0212 = (r64)a.m[1][0] * (r64)a.m[2][2] - (r64)a.m[1][2] * (r64)a.m[2][0];
    r64 A0113 = (r64)a.m[1][0] * (r64)a.m[3][1] - (r64)a.m[1][1] * (r64)a.m[3][0];
    r64 A0112 = (r64)a.m[1][0] * (r64)a.m[2][1] - (r64)a.m[1][1] * (r64)a.m[2][0];

    r64 det = ( (r64)a.m[0][0] * ((r64)a.m[1][1] * A2323 - (r64)a.m[1][2] * A1323 + (r64)a.m[1][3] * A1223) 
               -(r64)a.m[0][1] * ((r64)a.m[1][0] * A2323 - (r64)a.m[1][2] * A0323 + (r64)a.m[1][3] * A0223) 
               +(r64)a.m[0][2] * ((r64)a.m[1][0] * A1323 - (r64)a.m[1][1] * A0323 + (r64)a.m[1][3] * A0123) 
               -(r64)a.m[0][3] * ((r64)a.m[1][0] * A1223 - (r64)a.m[1][1] * A0223 + (r64)a.m[1][2] * A0123));
    det = 1.0 / det;

    result.m[0][0] = (r32)(det *  ((r64)a.m[1][1] * A2323 - (r64)a.m[1][2] * A1323 + (r64)a.m[1][3] * A1223));
    result.m[0][1] = (r32)(det * -((r64)a.m[0][1] * A2323 - (r64)a.m[0][2] * A1323 + (r64)a.m[0][3] * A1223));
    result.m[0][2] = (r32)(det *  ((r64)a.m[0][1] * A2313 - (r64)a.m[0][2] * A1313 + (r64)a.m[0][3] * A1213));
    result.m[0][3] = (r32)(det * -((r64)a.m[0][1] * A2312 - (r64)a.m[0][2] * A1312 + (r64)a.m[0][3] * A1212));
    result.m[1][0] = (r32)(det * -((r64)a.m[1][0] * A2323 - (r64)a.m[1][2] * A0323 + (r64)a.m[1][3] * A0223));
    result.m[1][1] = (r32)(det *  ((r64)a.m[0][0] * A2323 - (r64)a.m[0][2] * A0323 + (r64)a.m[0][3] * A0223));
    result.m[1][2] = (r32)(det * -((r64)a.m[0][0] * A2313 - (r64)a.m[0][2] * A0313 + (r64)a.m[0][3] * A0213));
    result.m[1][3] = (r32)(det *  ((r64)a.m[0][0] * A2312 - (r64)a.m[0][2] * A0312 + (r64)a.m[0][3] * A0212));
    result.m[2][0] = (r32)(det *  ((r64)a.m[1][0] * A1323 - (r64)a.m[1][1] * A0323 + (r64)a.m[1][3] * A0123));
    result.m[2][1] = (r32)(det * -((r64)a.m[0][0] * A1323 - (r64)a.m[0][1] * A0323 + (r64)a.m[0][3] * A0123));
    result.m[2][2] = (r32)(det *  ((r64)a.m[0][0] * A1313 - (r64)a.m[0][1] * A0313 + (r64)a.m[0][3] * A0113));
    result.m[2][3] = (r32)(det * -((r64)a.m[0][0] * A1312 - (r64)a.m[0][1] * A0312 + (r64)a.m[0][3] * A0112));
    result.m[3][0] = (r32)(det * -((r64)a.m[1][0] * A1223 - (r64)a.m[1][1] * A0223 + (r64)a.m[1][2] * A0123));
    result.m[3][1] = (r32)(det *  ((r64)a.m[0][0] * A1223 - (r64)a.m[0][1] * A0223 + (r64)a.m[0][2] * A0123));
    result.m[3][2] = (r32)(det * -((r64)a.m[0][0] * A1213 - (r64)a.m[0][1] * A0213 + (r64)a.m[0][2] * A0113));
    result.m[3][3] = (r32)(det *  ((r64)a.m[0][0] * A1212 - (r64)a.m[0][1] * A0212 + (r64)a.m[0][2] * A0112));

#if WINDY_INTERNAL
    m4 test_identity = a * result;
    Assert(equal(test_identity, identity_m4()));
#endif

    return result;
}

inline m4
flip_m4(i32 first_comp, i32 second_comp, i32 third_comp, i32 fourth_comp)
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
scale_m4(r32 sx, r32 sy, r32 sz)
{
    m4 matrix = {
         sx, 0.f, 0.f, 0.f,
        0.f,  sy, 0.f, 0.f,
        0.f, 0.f,  sz, 0.f,
        0.f, 0.f, 0.f, 1.f
    };
    return matrix;
}
inline m4 scale_m4(v3  s) { return scale_m4(s.x, s.y, s.z); }
inline m4 scale_m4(v2  s) { return scale_m4(s.x, s.y, 1.f); }
inline m4 scale_m4(r32 s) { return scale_m4(  s,   s,   s); }

inline m4
translation_m4(r32 tx, r32 ty, r32 tz)
{
    m4 matrix = {
        1.f, 0.f, 0.f, 0.f,
        0.f, 1.f, 0.f, 0.f,
        0.f, 0.f, 1.f, 0.f,
         tx,  ty,  tz, 1.f
    };
    return matrix;
}
inline m4 translation_m4(v3  t) { return translation_m4(t.x, t.y, t.z); }
inline m4 translation_m4(r32 t) { return translation_m4(  t,   t,   t); }

inline m4
pitch_m4(r32 delta) // Around X axis
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
roll_m4(r32 delta) // Around Y axis
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
yaw_m4(r32 delta) // Around Z axis
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
rotation_m4(v3 r)
{
    m4 matrix = pitch_m4(r.x) * roll_m4(r.y) * yaw_m4(r.z);
    return matrix;
}

inline m4
camera_m4(v3 pos, v3 target, v3 up)
{
    v3 n = normalize(target - pos);
    v3 v = normalize(cross(n, up));
    v3 u = normalize(cross(v, n));
    r32 p_v = -dot(pos, v);
    r32 p_u = -dot(pos, u);
    r32 p_n = -dot(pos, n);

    m4 matrix = {
        v.x, u.x, n.x, 0.f,
        v.y, u.y, n.y, 0.f,
        v.z, u.z, n.z, 0.f,
        p_v, p_u, p_n, 1.f
    };

    return matrix;
}

inline m4
perspective_m4(r32 fov, r32 ar, r32 n, r32 f)
{
    r32 cot = 1.f/Tan(fov/2.f);
    m4 matrix = {
        cot/ar, 0.f,       0.f, 0.f,
        0.f,    cot,       0.f, 0.f,
        0.f,    0.f,  -f/(n-f), 1.f,
        0.f,    0.f, n*f/(n-f), 0.f
    };
    return matrix;
}

inline m4
ortho_m4(r32 size, r32 ar, r32 n, r32 f)
{
    r32 inv = 1.f/size;
    // @todo: test this matrix
    m4 matrix = {
        inv/ar, 0.f,               0.f, 0.f,
        0.f,    inv,               0.f, 0.f,
        0.f,    0.f, 1.f/(f-n), 0.f,
        0.f,    0.f, (-n)/(f-n), 1.f
    };
    return matrix;
}

inline m4
transform_m4(v3 pos, v3 euler, v3 scale)
{
    //m4 matrix = Translation_m4(pos) * Rotation_m4(euler) * Scale_m4(scale);
    m4 matrix = scale_m4(scale);
    matrix.m[3][0] = pos.x;
    matrix.m[3][1] = pos.y;
    matrix.m[3][2] = pos.z;
    return matrix;
}

inline m4
world_to_local_m4(v3 u, v3 v, v3 n, v3 origin = {})
{
    m4 matrix = {
             u.x, v.x, n.x, 0.f,
             u.y, v.y, n.y, 0.f,
             u.z, v.z, n.z, 0.f,
             0.f, 0.f, 0.f, 1.f
    };
    matrix *= translation_m4(-origin);
    return matrix;
}

inline m4
no_translation_m4(m4 a)
{
    m4 result = a;
    result.col[3] = make_v4(0.f, 0.f, 0.f, 1.f);
    return result;
}

#define WINDY_MATH_H
#endif
