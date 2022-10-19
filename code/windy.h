#if !defined(WINDY_H)
/* ========================================================================
   $File: $
   $Date: $
   $Revision: $
   $Creator: Davide Stasio $
   $Notice: (C) Copyright 2020 by Davide Stasio. All Rights Reserved. $
   ======================================================================== */
#include "windy_platform.h"

#if 0
#define Continue_Here _continue_here
#else
#define Continue_Here 
#endif

#if WINDY_DEBUG
#define Assert(expr) if(!(expr)) {*(int *)0 = 0;}

#define do_once(expr) {local_persist bool _donce_ = false; \
                       if (!_donce_) { expr; _donce_ = true; }}
#else
#define Assert(expr)
#endif
#include "windy_math.h"

#define byte_offset(base, offset) ((u8*)(base) + (offset))
inline u16 truncate_to_u16(u32 v) {assert(v <= 0xFFFF); return (u16)v; };

#define Foru(a, b) for(u32 it = (a); it <= (b); it += 1)

struct Camera
{
    v3 pos;
    v3 target;
    v3 up;

    r32 fov;
    r32 min_z;
    r32 max_z;

    b32 is_ortho;
    r32 ortho_scale;

    r32 _radius; // Variables used for third person camera
    v3  _pivot;   // Used for editor camera
};

enum Entity_Type
{
    ENTITY_UNRESPONSIVE = 0,
    ENTITY_MOVABLE,

    ENTITY_UI,
};

struct Entity_Movable
{
    b32 physics_enabled;
    m4  transform;

    // @todo: p in entity base?
    v3  p;
    v3  dp;
    v3  ddp;
};

struct Entity
{
    Entity_Type            type;
    Platform_Mesh_Buffers  buffers;
    char                  *name;

    Entity_Movable movable;

#if WINDY_INTERNAL
    struct 
    {
        b32 is_init;
        v3 real_position; // used when moving the entity (in the editor)
                          // and snapping its position to steps;
                          // this is the "actual" position it's currently at.
    } _editor;
#endif
};

#define MAX_LEVEL_OBJECTS 20
#define MAX_LEVEL_LIGHTS  10
// @todo: Entity struct should keep all the info needed for rendering
struct Level
{
    // @todo: use pointers for meshes
    Entity objects[MAX_LEVEL_OBJECTS];
    Platform_Light_Buffer lights;
    u32  n_objects;
    u32  n_lights;

    Entity &last_obj() {return objects[n_objects - 1];};
};
 
struct Game_State
{
    Platform_Shader  *phong_shader;
    Platform_Shader  *font_shader;

    Level *current_level;
    Level *  gizmo_level;

    Entity *selected;
    b32     is_mouse_dragging; // true if mouse is being held down
    b32     editor_is_rotating_view;

    Entity *player;

    Platform_Texture  tex_white;
    Platform_Texture  tex_yellow;
    Platform_Texture  tex_sky;
    Platform_Font     inconsolata;

    m4 cam_matrix;
    m4 proj_matrix;

    Camera game_camera;
    Camera editor_camera;
};

struct Memory_Pool
{
    Memory_Index size;
    Memory_Index used;
    u8 *base;
};

#define push_struct(pool, type) (type *)push_size_((pool), sizeof(type))
#define push_array(pool, length, type) (type *)push_size_((pool), (length)*sizeof(type))
inline void *
push_size_(Memory_Pool *pool, Memory_Index size)
{
    Assert((pool->used + size) < pool->size);

    void *result = pool->base + pool->used;
    pool->used += size;
    return(result);
}

#define WINDY_H
#endif
