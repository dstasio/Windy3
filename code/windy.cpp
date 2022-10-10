/* ========================================================================
   $File: $
   $Date: $
   $Revision: $
   $Creator: Davide Stasio $
   $Notice: (C) Copyright 2020 by Davide Stasio. All Rights Reserved. $
   ======================================================================== */
#include "windy.h"
#include <string.h>
#include <cstdio>

#if WINDY_DEBUG
#define DEBUG_BUFFER_raycast  0
#define DEBUG_BUFFER_new     10
v3 DEBUG_buffer[DEBUG_BUFFER_new*2] = {};
#endif

r32 global_mouse_sensitivity = 50.f;

#define HEX_COLOR(hex) {(r32)(((hex) & 0xFF0000) >> 16) / 255.f, (r32)(((hex) & 0xFF00) >> 8) / 255.f, (r32)((hex) & 0xFF) / 255.f}

internal inline b32
string_compare(char *s1, char *s2)
{
    b32 result = 1;
    while((*s1) && (*s2) &&
          (*s1 == *s2))
    {
        s1 += 1;
        s2 += 1;
    }

    if ((*s1) ||
        (*s2))
    {
        result = 0;
    }
    return result;
}

internal void
mesh_move(Entity *mesh, v3 pos_delta)
{
    mesh->movable.p        += pos_delta;
    mesh->movable.transform = translation_m4(mesh->movable.p);
}

internal void
mesh_set_position(Entity *mesh, v3 pos)
{
    mesh->movable.p         = pos;
    mesh->movable.transform = translation_m4(mesh->movable.p);
}

internal void
editor_move_entity(Entity *entity, v3 pos_delta, b32 do_snap)
{
    Assert(entity->_editor.is_init);

    entity->_editor.real_position += pos_delta;

    if (do_snap) {
        entity->movable.p.x = floor(entity->_editor.real_position.x);
        entity->movable.p.y = floor(entity->_editor.real_position.y);
        entity->movable.p.z = floor(entity->_editor.real_position.z);
    }
    else {
        entity->movable.p = entity->_editor.real_position;
    }
    entity->movable.transform = translation_m4(entity->movable.p);
}

// @note: if settings is zero, phong flags is zero and the shader uses the texture
//        bound at rendering time
internal Level *
new_level(Memory_Pool *mempool, Platform_Renderer *renderer,
          Platform_Read_File *read_file, Platform_Close_File *close_file,
          char *path, Platform_Shader *shader = 0, Platform_Phong_Settings *settings = 0)
{
    // @todo: cleanup allocation for wexp file;
    //        use memory pool and keep the vertices for collision computation
    Level *level = push_struct(mempool, Level);
    Input_File wexp_file = read_file(path);
    Wexp_Header *wexp = (Wexp_Header *)wexp_file.data;

    Assert(wexp->signature != 0x7877) // 'wx': old wexp file
    Assert(wexp->version == 2);

    Assert(wexp->signature == 0x7857);  // 'Wx': new wexp file

    // wexp->mesh_count
    Wexp_Mesh_Header *mesh_header = (Wexp_Mesh_Header *)(wexp + 1);
    while(mesh_header->signature == 0x6D57)   // If signature is 'Wm', current object is a mesh
    {
        Entity *entity = &level->objects[level->n_objects];
        entity->buffers.vertex_data   = byte_offset(mesh_header, mesh_header->vertex_data_offset);
        entity->buffers. index_data   = byte_offset(mesh_header, mesh_header->index_data_offset);
        entity->buffers.vertex_count  = (mesh_header->index_data_offset - mesh_header->vertex_data_offset) / WEXP_VERTEX_SIZE;
        entity->buffers. index_count  = truncate_to_u16((mesh_header->name_offset  - mesh_header->index_data_offset) / WEXP_INDEX_SIZE);
        entity->buffers.vertex_stride = WEXP_VERTEX_SIZE;
        entity->name = (char *)byte_offset(mesh_header, mesh_header->name_offset);

        mesh_set_position(entity, mesh_header->world_position);

        entity->_editor.is_init = 1;
        entity->_editor.real_position = entity->movable.p;


        if (settings)
            entity->buffers.settings = *settings;

        renderer->init_mesh(&entity->buffers, shader);

        level->n_objects += 1;
        mesh_header = (Wexp_Mesh_Header *)byte_offset(mesh_header, mesh_header->next_elem_offset);
    }

    // @todo: we don't close the file because we point our mesh data to the file.
//    close_file(&wexp_file);
    return level;
}

// Searches for a mesh with the given name
// If none is found, returns 0
internal Entity *
find_mesh_checked(Level *level, char *name)
{
    Entity *result = 0;
    for (u32 mesh_index = 0;
         (!result) && (mesh_index < level->n_objects);
         ++mesh_index)
    {
        if (string_compare(name, level->objects[mesh_index].name))
            result = &level->objects[mesh_index];
    }

    Assert(result);
    return result;
}

internal Platform_Texture
load_bitmap(Memory_Pool *mempool, Platform_Read_File *read_file, char *path)
{
    Platform_Texture texture = {};
    Bitmap_Header *bmp = (Bitmap_Header *)read_file(path).data;
    texture.width  = bmp->Width;
    texture.height = bmp->Height;
    texture.size = texture.width*texture.height*4;
    texture.bytes = push_array(mempool, texture.size, u8);

    assert(bmp->BitsPerPixel == 24);
    assert(bmp->Compression == 0);

    u32 scanline_byte_count = texture.width*3;
    scanline_byte_count    += (4 - (scanline_byte_count & 0x3)) & 0x3;
    u32 r_mask = 0x000000FF;
    u32 g_mask = 0x0000FF00;
    u32 b_mask = 0x00FF0000;
    u32 a_mask = 0xFF000000;
    for(u32 y = 0; y < texture.height; ++y)
    {
        for(u32 x = 0; x < texture.width; ++x)
        {
            u32 src_offset  = (texture.height - y - 1)*scanline_byte_count + x*3;
            u32 dest_offset = y*texture.width*4 + x*4;
            u32 *src_pixel  = (u32 *)byte_offset(bmp, bmp->DataOffset + src_offset);
            u32 *dest_pixel = (u32 *)byte_offset(texture.bytes, dest_offset);
            *dest_pixel = (r_mask & (*src_pixel >> 16)) | (g_mask & (*src_pixel)) | (b_mask & (*src_pixel >> 8)) | a_mask;
        }
    }

    return texture;
}

inline Platform_Texture
load_texture(Platform_Renderer *renderer, Memory_Pool *mempool, Platform_Read_File *read_file, char *path)
{
    Platform_Texture texture = load_bitmap(mempool, read_file, path);
    renderer->init_texture(&texture);
    return texture;
}

inline Platform_Font *
load_font(Platform_Font *font, Platform_Read_File *read_file, char *path, r32 height)
{
    Input_File font_file = read_file(path);
    stbtt_InitFont(&font->info, font_file.data, stbtt_GetFontOffsetForIndex(font_file.data, 0));
    font->height = height;
    font->scale = stbtt_ScaleForPixelHeight(&font->info, height);
    return font;
}


#if 0 // @unused
void third_person_camera(Input *input, Camera *camera, v3 target, r32 dtime)
{
    camera->_yaw += input->mouse.dx*dtime;
    camera->_pitch -= input->mouse.dy*dtime;
    camera->_pitch  = Clamp(camera->_pitch, -PI/2.1f, PI/2.1f);
    camera->_radius -= input->mouse.wheel*0.1f*dtime;

    camera->pos.z  = Sin(camera->_pitch);
    camera->pos.x  = Cos(camera->_yaw) * Cos(camera->_pitch);
    camera->pos.y  = Sin(camera->_yaw) * Cos(camera->_pitch);
    camera->pos    = Normalize(camera->pos)*camera->_radius;
    camera->pos   += target;
    camera->target = target + make_v3(0.f, 0.f, 1.3f);
}
#endif

#if 0
struct Light_Buffer
{
    v3 color;
    r32 pad0_; // 16 bytes
    v3 p;      // 28 bytes
    r32 pad1_;
    v3 eye;    // 40 bytes
};
#endif

struct Raycast_Result
{
    b32 hit;
    r32 dist_sq;

    v3  impact_point;
    v3  normal;
};

// @todo: Better algorithm
// @todo: take Entity struct as parameter, and use its transform matrix
//
internal Raycast_Result
raycast(Entity *entity, v3 from, v3 dir, r32 min_distance, r32 max_distance)
{
    Raycast_Result result = {};

    v3 *verts    = (v3  *)entity->buffers.vertex_data;
    u16 *indices = (u16 *)entity->buffers.index_data;
    for (u32 i = 0; i < entity->buffers.index_count; i += 3)
    {
        v3 p1 = entity->movable.transform * (*((v3 *)byte_offset(verts, WEXP_VERTEX_SIZE*(indices[i  ]))));
        v3 p2 = entity->movable.transform * (*((v3 *)byte_offset(verts, WEXP_VERTEX_SIZE*(indices[i+1]))));
        v3 p3 = entity->movable.transform * (*((v3 *)byte_offset(verts, WEXP_VERTEX_SIZE*(indices[i+2]))));

        v3 n = normalize(cross(((p2) - (p1)), ((p3) - (p1))));
        if (dot(n, dir) < 0)
        {
            v3 u = normalize(cross( n, ((p2) - (p1))));
            v3 v = normalize(cross( u, n));
            m4 face_local_transform = world_to_local_m4(u, v, n, (p1));

            v3 local_start = face_local_transform * from;
            v3 local_dir   = no_translation_m4(face_local_transform) *  dir;
            r32 dir_steps  = - local_start.z / local_dir.z;
            if (dir_steps >= 0)
            {
                v3 local_incident_point = local_start + dir_steps*local_dir;

                v3 world_incident_point = ((local_incident_point.x * u) +
                                           (local_incident_point.y * v) +
                                           (local_incident_point.z * n) + (p1));

                v3 p1_p2 = (p2) - (p1);
                v3 p2_p3 = (p3) - (p2);
                v3 p3_p1 = (p1) - (p3);
                v3 p1_in = world_incident_point - (p1);
                v3 p2_in = world_incident_point - (p2);
                v3 p3_in = world_incident_point - (p3);
                if ((dot(cross(p1_p2, p1_in), n) > 0) &&
                    (dot(cross(p2_p3, p2_in), n) > 0) &&
                    (dot(cross(p3_p1, p3_in), n) > 0))
                {
                    r32 dist = length_sq(from - world_incident_point);

                    if ((dist > Square(min_distance)) && (dist < Square(max_distance)))
                    {
                        if (!result.hit || (dist < result.dist_sq))
                        {
                            result.dist_sq      = dist;
                            result.impact_point = world_incident_point;
                            result.hit          = true;
                            result.normal       = n;

#if WINDY_DEBUG
                            DEBUG_buffer[DEBUG_BUFFER_new+DEBUG_BUFFER_raycast+0] = world_incident_point;
                            DEBUG_buffer[DEBUG_BUFFER_new+DEBUG_BUFFER_raycast+1] = from;
                            DEBUG_buffer[DEBUG_BUFFER_new+DEBUG_BUFFER_raycast+2] = dir;
                            DEBUG_buffer[DEBUG_BUFFER_new+DEBUG_BUFFER_raycast+3] = p1;
                            DEBUG_buffer[DEBUG_BUFFER_new+DEBUG_BUFFER_raycast+4] = p2;
                            DEBUG_buffer[DEBUG_BUFFER_new+DEBUG_BUFFER_raycast+5] = p3;
#endif
                        }
                    }
                }
            }
        }
    }

    return result;
}

// @todo: move aspect ratio variable (maybe into Camera/Renderer struct)
void draw_level(Platform_Renderer *renderer, Level *level, Platform_Shader *shader, Camera *camera, r32 ar)
{
    m4 cam_space_transform    = camera_m4(camera->pos, camera->target, camera->up);
    m4 screen_space_transform = perspective_m4(camera->fov, ar, camera->min_z, camera->max_z);
    if (camera->is_ortho)
    {
        screen_space_transform = ortho_m4(camera->ortho_scale, ar, camera->min_z, camera->max_z);
    }

    renderer->draw_mesh(0, 0, shader, &cam_space_transform, &screen_space_transform, &level->lights, &camera->pos, 0, 1);

    for (Entity *mesh = level->objects; 
         (mesh - level->objects) < level->n_objects;
         mesh += 1)
    {
        renderer->draw_mesh(&mesh->buffers, &mesh->movable.transform, 0, 0, 0, 0, 0, 0, 1);
    }
}

#define gjk_farthest_minkowski(d, _first, _second)   (gjk_get_farthest_along_direction( (dir), (_first)) - gjk_get_farthest_along_direction(-(dir), (_second)))

// @todo: support rotations
internal v3
gjk_get_farthest_along_direction(v3 dir, Entity *m)
{
    void  *vertices = m->buffers.vertex_data;
    u32  n_vertices = m->buffers.vertex_count;
    u8     v_stride = m->buffers.vertex_stride;
    v3  farthest = ((v3*)vertices)[0];

    r32 max_distance = dot(dir, farthest);

    for(u32 index = 1; index < n_vertices; ++index)
    {
        v3 current_vertex = ((v3 *)byte_offset(vertices, (index*v_stride)))[0];
        r32 current_distance = dot(dir, current_vertex);
        if (current_distance > max_distance)
        {
            farthest     = current_vertex;
            max_distance = current_distance;
        }
    }

    farthest += m->movable.p;
    return farthest;
}

// @doc: returns true on intersection, false otherwise
//       fills parameters with new simplex data
#define same_direction(a, b)  (dot((a),(b)) > 0)
internal b32
gjk_do_simplex(v3 *simplex, u32 *simplex_count, v3 *out_dir)
{
    b32 result = 0;

    if (*simplex_count == 2)
    {
        v3 A = simplex[1];
        v3 B = simplex[0];
        v3 AO =  -A;
        v3 AB = B-A;

        //simplex remains unchanged
        *out_dir = cross(cross(AB, AO), AB);
    }
    else if(*simplex_count == 3)
    {
        v3 A = simplex[2];
        v3 B = simplex[1];
        v3 C = simplex[0];
        v3 AO =  -A;
        v3 AB = B-A;
        v3 AC = C-A;
        v3 ABC = cross(AB, AC);
        if (same_direction(cross(ABC,AC), AO))
        {
            if (same_direction(AC, AO))                  // AC closest
            {
                simplex[0] = C;
                simplex[1] = A;
                *simplex_count = 2;
                *out_dir = cross(AC, cross(AO, AC));
            }
            else if (same_direction(AB, AO))             // AB closest
            {
                simplex[0] = B;
                simplex[1] = A;
                *simplex_count = 2;
                *out_dir = cross(AB, cross(AO, AB));
            }
            else                                         // A closest
            {
                simplex[0] = A;
                *simplex_count = 1;
                *out_dir = AO;
            }
        }
        else if (same_direction(cross(AB, ABC), AO))
        {
            if (same_direction(AB, AO))                  // AB closest
            {
                simplex[0] = B;
                simplex[1] = A;
                *simplex_count = 2;
                *out_dir = cross(AB, cross(AO, AB));
            }
            else                                         // A closest
            {
                simplex[0] = A;
                *simplex_count = 1;
                *out_dir = AO;
            }
        }
        else if (same_direction(ABC, AO))                // ABC closest, in direction of ABC vector
        {
            simplex[0] = C;
            simplex[1] = B;
            simplex[2] = A;
            *simplex_count = 3;
            *out_dir = ABC;
        }
        else                                             // ABC closest, in direction of -ABC vector
        {
            simplex[0] = B;
            simplex[1] = C;
            simplex[2] = A;
            *simplex_count = 3;
            *out_dir = -ABC;
        }
    }
    else if(*simplex_count == 4)
    {
        // @todo @critical: fix the 4-vertex simplex case
        v3 A = simplex[3];
        v3 B = simplex[2];
        v3 C = simplex[1];
        v3 D = simplex[0];
        v3 AO =  -A;
        v3 AB = B-A;
        v3 AC = C-A;
        v3 AD = D-A;

        // @todo: remove need for direction checking
        v3 ABC = cross(AB, AC);
        if (!(same_direction(ABC, AD)))
            ABC = -ABC;

        v3 ABD = cross(AB, AD);
        if (!(same_direction(ABD, AC)))
            ABD = -ABD;

        v3 ACD = cross(AC, AD);
        if (!(same_direction(ACD, AB)))
            ACD = -ACD;

        if (same_direction(ABD, AO))
        {
            if (same_direction(ACD, AO))
            {
                if (same_direction(ABC, AO))
                {
                    result = 1;                                  // Intersection found!
                }
                else                                             // ABC closest, in direction of -ABC vector
                {
                    simplex[0] = B;
                    simplex[1] = C;
                    simplex[2] = A;
                    *simplex_count = 3;
                    *out_dir = -ABC;
                }
            }
            else
            {
                if (same_direction(ABC, AO))                     // ACD closest, in direction of -ACD vector
                {
                    simplex[0] = C;
                    simplex[1] = D;
                    simplex[2] = A;
                    *simplex_count = 3;
                    *out_dir = -ACD;
                }
                else                                             // AC closest
                {
                    simplex[0] = C;
                    simplex[1] = A;
                    *simplex_count = 2;
                    *out_dir = cross(AC, cross(AO, AC));
                }
            }
        }
        else
        {
            if (same_direction(ACD, AO))
            {
                if (same_direction(ABC, AO))                      // ABD closest, in direction of -ABD vector
                {
                    simplex[0] = B;
                    simplex[1] = D;
                    simplex[2] = A;
                    *simplex_count = 3;
                    *out_dir = -ABD;
                }
                else                                              // AB closest
                {
                    simplex[0] = B;
                    simplex[1] = A;
                    *simplex_count = 2;
                    *out_dir = cross(AB, cross(AO, AB));
                }
            }
            else                                                  // AD closest
            {
                simplex[0] = D;
                simplex[1] = A;
                *simplex_count = 2;
                *out_dir = cross(AD, cross(AO, AD));
            }
        }
    }

    return result;
}

// @todo: support rotations
internal b32
gjk_intersection(Entity *x, Entity *y)
{
    b32 intersection = 0;

    v3 dir = {0, 1, 0};
    v3  simplex[4];
    u32 simplex_count = 0;

    v3 f_x = gjk_get_farthest_along_direction( dir, x);
    v3 f_y = gjk_get_farthest_along_direction(-dir, y);
    simplex[simplex_count++] = f_x - f_y;
    //simplex[simplex_count++] = gjk_farthest_minkowski(dir,
    //                                    x->buffers.vertex_data, x->buffers.vertex_count, x->buffers.vertex_stride,
    //                                    y->buffers.vertex_data, y->buffers.vertex_count, y->buffers.vertex_stride);
    dir = -simplex[0];
    do {
        v3 P = gjk_farthest_minkowski(dir, x, y);
        if (dot(P, dir) < 0)
            break;                                                                // no intersection
        Assert(simplex_count <= 3);
        simplex[simplex_count++] = P;

        intersection = gjk_do_simplex(simplex, &simplex_count, &dir);             // intersection
    } while(!intersection);

    return intersection;
}

internal b32
check_mesh_collision(Entity *mesh, Level *level)
{
    for (u32 mesh_index = 0; mesh_index < level->n_objects; ++mesh_index)
    {
        auto *other_mesh = &level->objects[mesh_index];
        if (mesh == other_mesh) continue;
        if (gjk_intersection(mesh, other_mesh)) return true;
    }
    return false;
}


struct Screen_To_World_Result {
    v3 pos;
    v3 dir;
};

Screen_To_World_Result screen_space_to_world(Camera *camera, r32 screen_ar, v2 screen_point)
{
    // screen_point:
    //  (0, 0) -> Top Left
    //  (1, 1) -> Bot Right

    v2 clip_space_screen_point = {
         ((screen_point.x*2.f) - 1.f),
        -((screen_point.y*2.f) - 1.f),
    };

    // clip_space_screen_point:
    // (-1, -1) -> Bot Left
    // (+1, +1) -> Top Right

    Screen_To_World_Result result = {};

    if (camera->is_ortho) 
    {
        v3 forward = normalize(camera->target - camera->pos);
        v3 right   = normalize(cross(forward, camera->up));
        v3 up      = normalize(cross(right, forward));

        result.dir = forward;

        clip_space_screen_point.x *= camera->ortho_scale * screen_ar;
        clip_space_screen_point.y *= camera->ortho_scale;

        result.pos = camera->pos + (result.dir * camera->min_z) + (clip_space_screen_point.x*right + clip_space_screen_point.y*up);
    }
    else
    {
        v3 forward = normalize(camera->target - camera->pos);
        v3 right   = normalize(cross(forward, camera->up));
        v3 up      = normalize(cross(right, forward));

        // @todo: tangent here can be cached
        result.dir.x = clip_space_screen_point.x * Tan(camera->fov/2.f) * screen_ar;
        result.dir.y = clip_space_screen_point.y * Tan(camera->fov/2.f);
        result.dir.z =  1.f;
        result.dir  *= camera->min_z;

        result.dir = result.dir.x*right + result.dir.y*up + result.dir.z*forward;
        result.pos = camera->pos + result.dir;
        result.dir = normalize(result.dir);
    }

    return result;
}

// @todo: move gamemode to game layer
GAME_UPDATE_AND_RENDER(WindyUpdateAndRender)
{
    Game_State *state = (Game_State *)memory->main_storage;
    if(!memory->is_initialized)
    {
        Memory_Pool main_pool     = {};
        Memory_Pool volatile_pool = {};
        main_pool.base = (u8 *)memory->main_storage;
        main_pool.size = memory->main_storage_size;
        main_pool.used = sizeof(Game_State);
        volatile_pool.base = (u8 *)memory->volatile_storage;
        volatile_pool.size = memory->volatile_storage_size;
        volatile_pool.used = sizeof(Game_State);

        renderer->load_renderer(renderer);

        //
        // Shaders and meshes/entities
        //
        // @todo: remove need for pre-allocation
        state->phong_shader       = push_struct(&volatile_pool, Platform_Shader);
        state->font_shader        = push_struct(&volatile_pool, Platform_Shader);
        renderer->reload_shader(    state->     phong_shader, "phong");
        renderer->reload_shader(    state->      font_shader, "fonts");
        renderer->reload_shader(&renderer->     debug_shader, "debug");

        state->tex_white  = load_texture(renderer, &volatile_pool, memory->read_file, "assets/blockout_white.bmp");
        state->tex_yellow = load_texture(renderer, &volatile_pool, memory->read_file, "assets/blockout_yellow.bmp");
        renderer->init_square_mesh(state->font_shader);

        { // Editor Gizmo
            state->gizmo_level = new_level(&volatile_pool, renderer, memory->read_file, memory->close_file, "assets/debug/gizmo.wexp", state->phong_shader);

            Entity *arrow_x = find_mesh_checked(state->gizmo_level, "X");
            arrow_x->type                   = ENTITY_UI;
            arrow_x->buffers.settings.flags = (PHONG_FLAG_UNLIT | PHONG_FLAG_SOLIDCOLOR);
            arrow_x->buffers.settings.color = {.9f, .1f, 0.f};

            Entity *arrow_y = find_mesh_checked(state->gizmo_level, "Y");
            arrow_y->type                   = ENTITY_UI;
            arrow_y->buffers.settings.flags = (PHONG_FLAG_UNLIT | PHONG_FLAG_SOLIDCOLOR);
            arrow_y->buffers.settings.color = {.1f, .9f, 0.2f};

            Entity *arrow_z = find_mesh_checked(state->gizmo_level, "Z");
            arrow_z->type                   = ENTITY_UI;
            arrow_z->buffers.settings.flags = (PHONG_FLAG_UNLIT | PHONG_FLAG_SOLIDCOLOR);
            arrow_z->buffers.settings.color = {0.1f, .3f, .9f};
        }

        state->current_level = new_level(&volatile_pool, renderer, memory->read_file, memory->close_file, "assets/level_0.wexp", state->phong_shader);
        state->player        = find_mesh_checked(state->current_level, "Player");
        state->player->type = ENTITY_MOVABLE;
        state->player->movable.physics_enabled = 1;
        state->player->movable.p.z = 0.1f;

        load_font(&state->inconsolata, memory->read_file, "assets/Inconsolata.ttf", 32);

        //
        // camera set-up
        //
        // @todo: init camera _radius _pitch _yaw from position and target
        state->game_camera.pos         = {0.f, -30.f, 18.f};
        state->game_camera.target      = {0.f,  0.f, 0.f};
        state->game_camera.up          = {0.f,  0.f, 1.f};
        state->game_camera.fov         = DegToRad*50.f;
        state->game_camera.min_z       = 0.1f;
        state->game_camera.max_z       = 1000.f;
        state->game_camera.is_ortho    = 0;

        state->editor_camera.pos         = {-6.17f, 2.3f, 7.f};
        state->editor_camera.target      = {-4.f, 2.4f, 6.f};
        state->editor_camera.up          = { 0.f,  0.f, 1.f};
        state->editor_camera.fov         = DegToRad*60.f;
        state->editor_camera.min_z       = 0.01f;
        state->editor_camera.max_z       = 1000.f;
        state->editor_camera.is_ortho    = 0;
        state->editor_camera.ortho_scale = 20.f;
        state->editor_camera._radius     = 2.5f;
        state->editor_camera._pitch      = 0.453010529f;
        state->editor_camera._yaw        = 2.88975f;
        //state->editor_camera._pivot      = state->player->movable.p;

        state->current_level->lights.light_count = 0;

        state->current_level->lights.type [0].t = PHONG_LIGHT_DIRECTIONAL;
        state->current_level->lights.color[0] = {0.9f,  0.9f, 0.9f};
        state->current_level->lights.pos  [0] = make_v4(normalize({0.3f, 0.9f, -1.0f})); // this is actually direction
        state->current_level->lights.light_count += 1;

        state->current_level->lights.type [1].t = PHONG_LIGHT_DIRECTIONAL;
        state->current_level->lights.color[1] = {0.2f,  0.2f, 0.17f};
        state->current_level->lights.pos  [1] = make_v4(normalize({-0.3f, -0.9f, 1.0f})); // this is actually direction
        state->current_level->lights.light_count += 1;

        state->current_level->lights.type [2].t = PHONG_LIGHT_POINT;
        state->current_level->lights.color[2] = {0.3f,  0.07f, 0.1f};
        state->current_level->lights.pos  [2] = {0.f, 5.f, 7.f};
        state->current_level->lights.light_count += 1;

        state->current_level->lights.type [3].t = PHONG_LIGHT_POINT;
        state->current_level->lights.color[3] = {0.05f,  0.1f, 0.23f};
        state->current_level->lights.pos  [3] = {15.f, 0.f, 5.f};
        state->current_level->lights.light_count += 1;

        memory->is_initialized = true;
    }

    //
    // ---------------------------------------------------------------
    //

    Camera *active_camera = 0;
    { // Input Processing.
        input->mouse.dp *= global_mouse_sensitivity;
        if (*gamemode == GAMEMODE_GAME) 
        {
            Entity *player = state->player;
            local_persist r32 jump_time_accumulator = 0.f;

            active_camera = &state->game_camera;

            v3 cam_forward = state->game_camera.target - state->game_camera.pos;
            v3 cam_right   = cross(cam_forward, state->game_camera.up);
            player->movable.ddp = {};
            if (input->held.up)     player->movable.ddp += 150.f * normalize(make_v3(cam_forward.xy));
            if (input->held.down)   player->movable.ddp -= 150.f * normalize(make_v3(cam_forward.xy));
            if (input->held.right)  player->movable.ddp += 150.f * normalize(make_v3(cam_right.xy));
            if (input->held.left)   player->movable.ddp -= 150.f * normalize(make_v3(cam_right.xy));

#define GRAVITY       400.f
#define JUMP_FORCE    900.f
#define MAX_JUMP_TIME   0.2f

            if (input->held.space) {
                player->movable.ddp += JUMP_FORCE * make_v3(0.f, 0.f, 1.f);
                jump_time_accumulator += dtime;
            }

            if (jump_time_accumulator >= MAX_JUMP_TIME)  player->movable.ddp.z = 0.f;

            player->movable.ddp.z -= GRAVITY;

            // simulating player physics
            if (player->movable.physics_enabled)
            {
                // @todo: movement equation
                v3 player_prev_p = player->movable.p;

                player->movable.dp += (player->movable.ddp - player->movable.dp * 9.f) * dtime;
                mesh_move(player, player->movable.dp * dtime);

                // Ground raycast
                bool is_on_ground = true;
                Raycast_Result least_hit = {};
                {
                    for (Entity *level_mesh = state->current_level->objects; 
                         (level_mesh - state->current_level->objects) < state->current_level->n_objects;
                         level_mesh += 1)
                    {
                        if (level_mesh == player) continue;

#define GROUND_MARGIN 0.01f
                        Raycast_Result hit = raycast(level_mesh, player_prev_p, {0.f, 0.f, -1.f}, 0.f, player->movable.p.z - player_prev_p.z);
                        if ((hit.hit) && (!least_hit.hit || (hit.dist_sq < least_hit.dist_sq)))
                        {
                            least_hit = hit;

#if 0 && WINDY_DEBUG
                            DEBUG_buffer[DEBUG_BUFFER_raycast+0] = DEBUG_buffer[DEBUG_BUFFER_new+DEBUG_BUFFER_raycast+0];
                            DEBUG_buffer[DEBUG_BUFFER_raycast+1] = DEBUG_buffer[DEBUG_BUFFER_new+DEBUG_BUFFER_raycast+1];
                            DEBUG_buffer[DEBUG_BUFFER_raycast+2] = DEBUG_buffer[DEBUG_BUFFER_new+DEBUG_BUFFER_raycast+2];
                            DEBUG_buffer[DEBUG_BUFFER_raycast+3] = DEBUG_buffer[DEBUG_BUFFER_new+DEBUG_BUFFER_raycast+3];
                            DEBUG_buffer[DEBUG_BUFFER_raycast+4] = DEBUG_buffer[DEBUG_BUFFER_new+DEBUG_BUFFER_raycast+4];
                            DEBUG_buffer[DEBUG_BUFFER_raycast+5] = DEBUG_buffer[DEBUG_BUFFER_new+DEBUG_BUFFER_raycast+5];
#endif
                        }
                    }

                    is_on_ground = least_hit.hit;
                }

                if (player->movable.ddp.z < 0.f && is_on_ground)
                {
                    player->movable.p.z = player_prev_p.z - Sqrt(least_hit.dist_sq) + GROUND_MARGIN;
                    player->movable.dp.z = 0.f;
                    jump_time_accumulator = 0.f;
                }

                if (jump_time_accumulator >= MAX_JUMP_TIME) {
                    //jump_time_accumulator = 0.f;
                }

            }

            active_camera->pos.x = player->movable.p.x;
            active_camera->target.x = player->movable.p.x;
        }
#if WINDY_INTERNAL
        else if (*gamemode == GAMEMODE_EDITOR)
        {
            active_camera = &state->editor_camera;
            local_persist v3 move_mask = {};
            local_persist v3 moving_start_position = {};

            Screen_To_World_Result deprojected_mouse = screen_space_to_world(active_camera, ((r32)width/(r32)height), input->mouse.p);
            r32 debug_arrow_dist_sq = 0.f;

            Raycast_Result current_mouse_hit          = {};
            Entity        *current_entity_under_mouse =  0;
            { // getting entity under mouse
                for (Entity *entity = state->current_level->objects; 
                     (entity - state->current_level->objects) < state->current_level->n_objects;
                     entity += 1)
                {
                    Raycast_Result hit = raycast(entity, deprojected_mouse.pos, deprojected_mouse.dir, active_camera->min_z, active_camera->max_z);
                    if ((hit.hit) && (!current_mouse_hit.hit || (hit.dist_sq < current_mouse_hit.dist_sq)))
                    {
                        current_entity_under_mouse = entity;
                        current_mouse_hit          = hit;

#if WINDY_DEBUG
                        DEBUG_buffer[DEBUG_BUFFER_raycast+0] = DEBUG_buffer[DEBUG_BUFFER_new+DEBUG_BUFFER_raycast+0];
                        DEBUG_buffer[DEBUG_BUFFER_raycast+1] = DEBUG_buffer[DEBUG_BUFFER_new+DEBUG_BUFFER_raycast+1];
                        DEBUG_buffer[DEBUG_BUFFER_raycast+2] = DEBUG_buffer[DEBUG_BUFFER_new+DEBUG_BUFFER_raycast+2];
                        DEBUG_buffer[DEBUG_BUFFER_raycast+3] = DEBUG_buffer[DEBUG_BUFFER_new+DEBUG_BUFFER_raycast+3];
                        DEBUG_buffer[DEBUG_BUFFER_raycast+4] = DEBUG_buffer[DEBUG_BUFFER_new+DEBUG_BUFFER_raycast+4];
                        DEBUG_buffer[DEBUG_BUFFER_raycast+5] = DEBUG_buffer[DEBUG_BUFFER_new+DEBUG_BUFFER_raycast+5];
#endif
                    }
                }
            }


            if (state->selected)
            {
                if (input->pressed.g)
                {
                    move_mask = make_v3(!move_mask);
                    if (move_mask)  moving_start_position = state->selected->movable.p;
                }

                Assert(state->selected->type != ENTITY_UI);

                // @todo: for now we manually trigger raycasts against entities which are not in the level.
                //        should this be changed?
                bool arrow_already_hit = false;
                for (Entity *entity = state->gizmo_level->objects; 
                     (entity - state->gizmo_level->objects) < state->gizmo_level->n_objects;
                     entity += 1)
                {
                    entity->movable.transform = state->selected->movable.transform;

                    if (arrow_already_hit)
                        continue;

                    Raycast_Result hit = raycast(entity, deprojected_mouse.pos, deprojected_mouse.dir, active_camera->min_z, active_camera->max_z);
                    if (hit.hit)
                    {
                        debug_arrow_dist_sq = hit.dist_sq;

                        if (input->held.mouse_left && !state->is_mouse_dragging) {
                            if      (string_compare("X", entity->name))
                                move_mask = {1.f, 0.f, 0.f};
                            else if (string_compare("Y", entity->name))
                                move_mask = {0.f, 1.f, 0.f};
                            else if (string_compare("Z", entity->name))
                                move_mask = {0.f, 0.f, 1.f};
                            else { Assert(0); }
                        }

                        arrow_already_hit = true;
                    }
                }
            }

            if (move_mask)
            {
                if (input->pressed.x)  move_mask = {1, 0, 0};
                if (input->pressed.y)  move_mask = {0, 1, 0};
                if (input->pressed.z)  move_mask = {0, 0, 1};

                if (input->pressed.mouse_right)
                {
                    move_mask = {};
                    state->selected->movable.p = moving_start_position;
                    state->selected->movable.transform = translation_m4(state->selected->movable.p);
                }
                else if (input->pressed.mouse_left)
                {
                    move_mask = {};
                }
                else
                {
                    v3 forward = normalize(active_camera->target - active_camera->pos);
                    v3 right   = normalize(cross(forward, active_camera->up));
                    v3 up      = normalize(cross(right, forward));

                    v3 movement = input->mouse.dx*right - input->mouse.dy*up;
                    movement /= dot((state->selected->movable.p - active_camera->pos), forward);
                    movement.x *= move_mask.x;
                    movement.y *= move_mask.y;
                    movement.z *= move_mask.z;
                    if (input->held.shift)
                        movement *= 0.1f;
                    editor_move_entity(state->selected, movement, input->held.ctrl);
                }
            }
            else
            {
                if (input->pressed.space)
                {
                    active_camera->is_ortho = !active_camera->is_ortho;
                }
                if (input->pressed.mouse_left && !input->held.alt)
                {
                    Entity *_prev_selected = state->selected;

                    state->selected = current_entity_under_mouse;
                }
            }

            if (input->held.mouse_middle || (input->held.alt && input->held.mouse_left))
            {
                if (!state->editor_is_rotating_view)
                {
#if 0
                    Raycast_Result hit = raycast(entity, deprojected_mouse.pos, deprojected_mouse.dir, active_camera->min_z, active_camera->max_z);
                    if (hit.hit)
                        active_camer->target = hit.impact_point;
#endif

                    state->editor_is_rotating_view = true;
                }

                if (input->held.shift)
                {
                    v3 forward = normalize(active_camera->target - active_camera->pos);
                    v3 right   = normalize(cross(forward, active_camera->up));
                    v3 up      = normalize(cross(right, forward));

                    r32 speed = 5.f;
                    active_camera->target += input->mouse.dy*up*dtime*speed - input->mouse.dx*right*dtime*speed;
                    active_camera->pos    += input->mouse.dy*up*dtime*speed - input->mouse.dx*right*dtime*speed;
                }
                else
                {
                    active_camera->_yaw   -= input->mouse.dx*PI*dtime;
                    active_camera->_pitch += input->mouse.dy*dtime;
                    //            active_camera->_pitch  = Clamp(active_camera->_pitch, -PI/2.1f, PI/2.1f);
                    active_camera->_radius -= input->mouse.wheel*0.1f*dtime;
                }
            }
            else
            {
                state->editor_is_rotating_view = false;

                if (input->mouse.wheel) {
                    if (active_camera->is_ortho)
                    {
                        active_camera->ortho_scale -= input->mouse.wheel*dtime;
                        active_camera->ortho_scale = clamp(active_camera->ortho_scale, 1.f, 100.f);
                    }
                    else
                    {
                        v3 forward = normalize(active_camera->target - active_camera->pos);
                        active_camera->_radius -= input->mouse.wheel*dtime;
                    }
                }
            }

            active_camera->pos.z  = Sin(active_camera->_pitch);
            active_camera->pos.x  = Cos(active_camera->_yaw) * Cos(active_camera->_pitch);
            active_camera->pos.y  = Sin(active_camera->_yaw) * Cos(active_camera->_pitch);
            active_camera->pos    = normalize(active_camera->pos)*active_camera->_radius;
            active_camera->pos   += active_camera->target;

        }
#endif // WINDY_INTERNAL
        else //if (*gamemode == GAMEMODE_MENU)
        {
        }

        state->is_mouse_dragging = input->held.mouse_left;
    } // End Input Processing.

#if WINDY_INTERNAL
    renderer->reload_shader(    state->     phong_shader, "phong");
    renderer->reload_shader(    state->      font_shader, "fonts");
    renderer->reload_shader(&renderer->     debug_shader, "debug");
#endif // WINDY_INTERNAL
    renderer->set_render_targets();

    renderer->clear(CLEAR_COLOR|CLEAR_DEPTH, HEX_COLOR(0x80d5f2), 1.f, 1);

    renderer->set_active_texture(&state->tex_white);

    draw_level(renderer, state->current_level, state->phong_shader, active_camera, (r32)width/(r32)height);
#if WINDY_INTERNAL
    if (*gamemode == GAMEMODE_EDITOR)
    {
        if (state->selected)
        {
            renderer->draw_mesh(&state->selected->buffers, &state->selected->movable.transform, state->phong_shader, 0, 0, 0, 0, 1, true);

            for (Entity *entity = state->gizmo_level->objects; 
                 (entity - state->gizmo_level->objects) < state->gizmo_level->n_objects;
                 entity += 1)
            {
                entity->movable.transform = state->selected->movable.transform;

                renderer->draw_mesh(&entity->buffers, &entity->movable.transform, state->phong_shader, 0, 0, 0, 0, 0, false);

            }
        }
#endif // WINDY_INTERNAL

#if 0
        { // Debug Light visualization
            Platform_Phong_Settings prev_settings = state->player->buffers.settings;
            state->player->buffers.settings.flags = PHONG_FLAG_UNLIT | PHONG_FLAG_SOLIDCOLOR;
            Foru(0, state->current_level->lights.light_count - 1) {
                if (state->current_level->lights.type[it].t != PHONG_LIGHT_POINT) continue;

                state->player->buffers.settings.color = make_v3(state->current_level->lights.color[it]);
                m4 transform = transform_m4(make_v3(state->current_level->lights.pos[it]), {}, {0.5f, 0.5f, 0.5f});

                renderer->draw_mesh(&state->player->buffers, &transform, state->phong_shader, 0, 0, 0, 0, 1, 1);
            }
            state->player->buffers.settings = prev_settings;
        }
#endif
    }


    char debug_text[128] = {};
    snprintf(debug_text, 128, "FPS: %.2f, X: %.2f\n"
                              "            Y: %.2f", 1.f/dtime, input->mouse.p.x, input->mouse.p.y);
#if 0
    if (check_mesh_collision(state->player, state->current_level))
        snprintf(debug_text, 128, "FPS: %.2f INTERSECTION", 1.f/dtime);
#endif
    renderer->draw_text(state->font_shader, &state->inconsolata, debug_text, make_v2(0, 0));

    {
        m4 camera = camera_m4(active_camera->pos, active_camera->target, active_camera->up);
        m4 screen = perspective_m4(active_camera->fov, (r32)width/(r32)height, active_camera->min_z, active_camera->max_z);
        if (active_camera->is_ortho)
        {
            screen = ortho_m4(active_camera->ortho_scale, (r32)width/(r32)height, active_camera->min_z, active_camera->max_z);
        }

#if WINDY_DEBUG
        v4 lc = {0.5f, 0.5f, 0.9f, 0.2f};
        renderer->draw_line(DEBUG_buffer[DEBUG_BUFFER_raycast+0], DEBUG_buffer[DEBUG_BUFFER_raycast+1], lc, 0, &camera, &screen);
        lc.w = 1.f;
        renderer->draw_line(DEBUG_buffer[DEBUG_BUFFER_raycast+1], DEBUG_buffer[DEBUG_BUFFER_raycast+1]+DEBUG_buffer[DEBUG_BUFFER_raycast+2]*200.f, lc, 0, 0, 0);
        lc = {0.3f, 0.6f, 0.3f, 0.9f};
        renderer->draw_line(DEBUG_buffer[DEBUG_BUFFER_raycast+0], DEBUG_buffer[DEBUG_BUFFER_raycast+3], lc, 1, 0, 0);
        renderer->draw_line(DEBUG_buffer[DEBUG_BUFFER_raycast+0], DEBUG_buffer[DEBUG_BUFFER_raycast+4], lc, 1, 0, 0);
        renderer->draw_line(DEBUG_buffer[DEBUG_BUFFER_raycast+0], DEBUG_buffer[DEBUG_BUFFER_raycast+5], lc, 1, 0, 0);

        lc = {0.7f, 0.6f, 0.3f, 0.9f};
        renderer->draw_line(DEBUG_buffer[DEBUG_BUFFER_raycast+3], DEBUG_buffer[DEBUG_BUFFER_raycast+4], lc, 1, 0, 0);
        renderer->draw_line(DEBUG_buffer[DEBUG_BUFFER_raycast+4], DEBUG_buffer[DEBUG_BUFFER_raycast+5], lc, 1, 0, 0);
        renderer->draw_line(DEBUG_buffer[DEBUG_BUFFER_raycast+5], DEBUG_buffer[DEBUG_BUFFER_raycast+3], lc, 1, 0, 0);
#endif
    }
}
