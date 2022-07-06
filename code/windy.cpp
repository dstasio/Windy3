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
//global Mesh *mesh_A;
//global Mesh *mesh_B;
//global Mesh *mesh_C;

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
mesh_move(Mesh *mesh, v3 pos_delta)
{
    mesh->p        += pos_delta;
    mesh->transform = translation_m4(mesh->p);
}

internal void
mesh_set_position(Mesh *mesh, v3 pos)
{
    mesh->p         = pos;
    mesh->transform = translation_m4(mesh->p);
}

internal void 
mesh_simulate_physics(Mesh *mesh, r32 dt)
{
    // @todo: maybe this function should be called only _when_ physics is enabled?
    if (!mesh->physics_enabled) return;

    // @todo: movement equation
    mesh->dp += (mesh->ddp - mesh->dp * 9.f) * dt;
    mesh_move(mesh, mesh->dp * dt);
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
    if (wexp->signature == 0x7857)  // 'Wx'
    {
        if (wexp->version < 3)
        {
            // wexp->mesh_count
            Wexp_Mesh_Header *mesh_header = (Wexp_Mesh_Header *)(wexp + 1);
            while(mesh_header->signature == 0x6D57)   // If signature is 'Wm', current object is a mesh
            {
                Mesh *mesh = &level->objects[level->n_objects];
                mesh->buffers.vertex_data  = byte_offset(mesh_header, mesh_header->vertex_data_offset);
                mesh->buffers.index_data   = byte_offset(mesh_header, mesh_header->index_data_offset);
                mesh->buffers.vertex_count = (mesh_header->index_data_offset - mesh_header->vertex_data_offset) / WEXP_VERTEX_SIZE;
                mesh->buffers.index_count  = truncate_to_u16((mesh_header->name_offset  - mesh_header->index_data_offset) / WEXP_INDEX_SIZE);
                mesh->buffers.vertex_stride = WEXP_VERTEX_SIZE;
                mesh->name = (char *)byte_offset(mesh_header, mesh_header->name_offset);

                if (wexp->version == 2)
                {
                    mesh_set_position(mesh, mesh_header->world_position);
                }
                else
                {
                    mesh->transform  = identity_m4();
                }

                if (settings)
                    mesh->buffers.settings = *settings;

                renderer->init_mesh(&mesh->buffers, shader);

                level->n_objects += 1;
                mesh_header = (Wexp_Mesh_Header *)byte_offset(mesh_header, mesh_header->next_elem_offset);
            }
        }
        else
        {
            // @todo: Log
        }
    }
    else if (wexp->signature == 0x7877) // 'wx'
    {
        // Legacy wexp file
        Assert(0);
    }
    else
    {
        // @todo: Log
    }

//    close_file(&wexp_file);
    return level;
}

// Searches for a mesh with the given name
// If none is found, returns 0
internal Mesh *
find_mesh(Level *level, char *name)
{
    Mesh *result = 0;
    for (u32 mesh_index = 0;
         (!result) && (mesh_index < level->n_objects);
         ++mesh_index)
    {
        if (string_compare(name, level->objects[mesh_index].name))
            result = &level->objects[mesh_index];
    }
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

void editor_camera(Input *input, Camera *camera, r32 dtime)
{
    if (input->held.mouse_middle)
    {
        if (input->held.shift)
        {
            v3 forward = normalize(camera->target - camera->pos);
            v3 right   = normalize(cross(forward, camera->up));
            v3 up      = normalize(cross(right, forward));

            r32 speed = 5.f;
            camera->target += input->mouse.dy*up*dtime*speed - input->mouse.dx*right*dtime*speed;
            camera->pos    += input->mouse.dy*up*dtime*speed - input->mouse.dx*right*dtime*speed;
        }
        else
        {
            camera->_yaw -= input->mouse.dx*PI*dtime;
            camera->_pitch += input->mouse.dy*dtime;
//            camera->_pitch  = Clamp(camera->_pitch, -PI/2.1f, PI/2.1f);
            camera->_radius -= input->mouse.wheel*0.1f*dtime;
        }
    }
    else
    {
        v3 forward = normalize(camera->target - camera->pos);
        camera->target += input->mouse.wheel*dtime*forward;
        camera->pos    += input->mouse.wheel*dtime*forward;
    }

    camera->pos.z  = Sin(camera->_pitch);
    camera->pos.x  = Cos(camera->_yaw) * Cos(camera->_pitch);
    camera->pos.y  = Sin(camera->_yaw) * Cos(camera->_pitch);
    camera->pos    = normalize(camera->pos)*camera->_radius;
    camera->pos   += camera->target;
}

#if 1
struct Light_Buffer
{
    v3 color;
    r32 pad0_; // 16 bytes
    v3 p;      // 28 bytes
    r32 pad1_;
    v3 eye;    // 40 bytes
};
#endif

// @todo: Better algorithm
// @todo: take Mesh struct as parameter, and use its transform matrix
//
internal r32
raycast(Mesh *mesh, v3 from, v3 dir, r32 min_distance, r32 max_distance)
{
    r32 hit_sq = 0.f;

    v3 *verts    = (v3  *)mesh->buffers.vertex_data;
    u16 *indices = (u16 *)mesh->buffers.index_data;
    for (u32 i = 0; i < mesh->buffers.index_count; i += 3)
    {
        v3 p1 = mesh->transform * (*((v3 *)byte_offset(verts, WEXP_VERTEX_SIZE*(indices[i  ]))));
        v3 p2 = mesh->transform * (*((v3 *)byte_offset(verts, WEXP_VERTEX_SIZE*(indices[i+1]))));
        v3 p3 = mesh->transform * (*((v3 *)byte_offset(verts, WEXP_VERTEX_SIZE*(indices[i+2]))));

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
                    r32 dist = length_Sq(from - world_incident_point);

                    if ((dist > Square(min_distance)) && (dist < Square(max_distance)))
                    {
                        if (!hit_sq || (dist < hit_sq))
                        {
                            hit_sq = dist;

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

    return Sqrt(hit_sq);
}

// @todo: move aspect ratio variable (maybe into Camera/Renderer struct)
void draw_level(Platform_Renderer *renderer, Level *level, Platform_Shader *shader, Camera *camera, r32 ar)
{
    m4 cam_space_transform    = camera_m4(camera->pos, camera->target, camera->up);
    m4 screen_space_transform = perspective_m4(camera->fov, ar, camera->min_z, camera->max_z);
    if (camera->is_ortho)
    {
        camera->ortho_scale = 5.f;
        camera->max_z = 500.f;
        screen_space_transform = ortho_m4(camera->ortho_scale, ar, camera->min_z, camera->max_z);
    }

    renderer->draw_mesh(0, 0, shader, &cam_space_transform, &screen_space_transform, &level->lights[0], &camera->pos, 0);

    for (Mesh *mesh = level->objects; 
         (mesh - level->objects) < level->n_objects;
         mesh += 1)
    {
        renderer->draw_mesh(&mesh->buffers, &mesh->transform, 0, 0, 0, 0, 0, 0);
    }
}

#define gjk_farthest_minkowski(d, _first, _second)   (gjk_get_farthest_along_direction( (dir), (_first)) - gjk_get_farthest_along_direction(-(dir), (_second)))

// @todo: support rotations
internal v3
gjk_get_farthest_along_direction(v3 dir, Mesh *m)
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

    farthest += m->p;
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
gjk_intersection(Mesh *x, Mesh *y)
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
check_mesh_collision(Mesh *mesh, Level *level)
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
    Screen_To_World_Result result = {};

    v3 forward = normalize(camera->target - camera->pos);
    v3 right   = normalize(cross(forward, camera->up));
    v3 up      = normalize(cross(right, forward));

    result.dir.x =  ((screen_point.x*2.f) - 1.f) * Tan(camera->fov/2.f) * screen_ar;
    result.dir.y = -((screen_point.y*2.f) - 1.f) * Tan(camera->fov/2.f);
    result.dir.z =  1.f;
    result.dir  *= camera->min_z;

    result.dir = result.dir.x*right + result.dir.y*up + result.dir.z*forward;
    result.pos = camera->pos + result.dir;
    result.dir = normalize(result.dir);

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
        // Shaders
        //
        // @todo: remove need for pre-allocation
        state->phong_shader       = push_struct(&volatile_pool, Platform_Shader);
        state->font_shader        = push_struct(&volatile_pool, Platform_Shader);
        state->background_shader  = push_struct(&volatile_pool, Platform_Shader);
        renderer->reload_shader(    state->     phong_shader, "phong");
        renderer->reload_shader(    state->      font_shader, "fonts");
        renderer->reload_shader(    state->background_shader, "background");
        renderer->reload_shader(&renderer->     debug_shader, "debug");

        Platform_Phong_Settings phong_settings = {};
        phong_settings.flags = PHONG_FLAG_SOLIDCOLOR;
        phong_settings.color = {0.2f, 0.3f, 0.6f};
        //load_mesh(renderer, memory->read_file, "assets/testpoly.wexp", &state->current_level, state->phong_shader, &phong_settings);
        //state->env    = load_mesh(renderer, memory->read_file, "assets/environment.wexp", &state->current_level, state->phong_shader);
        phong_settings.flags = PHONG_FLAG_SOLIDCOLOR;
        phong_settings.color = {0.8f, 0.f, 0.2f};
        //state->player = load_mesh(renderer, memory->read_file, "assets/player.wexp",      &state->current_level, state->phong_shader, &phong_settings);
        state->tex_white  = load_texture(renderer, &volatile_pool, memory->read_file, "assets/blockout_white.bmp");
        state->tex_yellow = load_texture(renderer, &volatile_pool, memory->read_file, "assets/blockout_yellow.bmp");
        state->tex_sky    = load_texture(renderer, &volatile_pool, memory->read_file, "assets/spiaggia_di_mondello_1k.bmp");
        renderer->init_square_mesh(state->font_shader);

        state->current_level = new_level(&volatile_pool, renderer, memory->read_file, memory->close_file, "assets/level_0.wexp", state->phong_shader);
        state->player = find_mesh(state->current_level, "Player");
        state->player->physics_enabled = 1;
        //mesh_A = find_mesh(state->current_level, "Cube_A");
        //mesh_B = find_mesh(state->current_level, "Cube_B");
        //mesh_C = find_mesh(state->current_level, "Cube_C");
        //Assert(mesh_A);
        //Assert(mesh_B);
        //Assert(mesh_C);

        load_font(&state->inconsolata, memory->read_file, "assets/Inconsolata.ttf", 32);

        //
        // camera set-up
        //
        // @todo: init camera _radius _pitch _yaw from position and target
        state->game_camera.pos         = {0.f, -17.f, 11.f};
        state->game_camera.target      = {0.f,  0.f, 0.f};
        state->game_camera.up          = {0.f,  0.f, 1.f};
        state->game_camera.fov         = DegToRad*50.f;
        state->game_camera.min_z       = 0.1f;
        state->game_camera.max_z       = 1000.f;
        state->game_camera.is_ortho    = 0;

        state->editor_camera.pos         = {0.f, -3.f, 2.f};
        state->editor_camera.target      = {0.f,  0.f, 0.f};
        state->editor_camera.up          = {0.f,  0.f, 1.f};
        state->editor_camera.fov         = DegToRad*60.f;
        state->editor_camera.min_z       = 0.01f;
        state->editor_camera.max_z       = 100.f;
        state->editor_camera.is_ortho    = 0;
        state->editor_camera.ortho_scale = 20.f;
        state->editor_camera._radius     = 2.5f;
        state->editor_camera._pitch      = 1.f;
        state->editor_camera._yaw        = PI/2.f;
        state->editor_camera._pivot      = state->player->p;

        //state->sun.color = {1.f,  1.f,  1.f};
        //state->sun.dir   = {0.f, -1.f, -1.f};
        state->current_level->lights[0].color = {1.f,  1.f, 0.9f};
        state->current_level->lights[0].p     = {0.f, -5.f, 7.f};
        memory->is_initialized = true;
    }

    //
    // ---------------------------------------------------------------
    //

    Camera *active_camera = 0;
    { // Input Processing.
        input->mouse.dp *= global_mouse_sensitivity;
//        input->mouse.pos *= global_mouse_sensitivity;
        if (*gamemode == GAMEMODE_GAME) 
        {
            active_camera = &state->game_camera;

            r32 speed = input->held.space ? 10.f : 3.f;
            v3  cam_forward = state->game_camera.target - state->game_camera.pos;
            v3  cam_right   = cross(cam_forward, state->game_camera.up);
            state->player->ddp = {};
            if (input->held.w)     state->player->ddp += 100.f * normalize(make_v3(cam_forward.xy));
            if (input->held.s)     state->player->ddp -= 100.f * normalize(make_v3(cam_forward.xy));
            if (input->held.d)     state->player->ddp += 100.f * normalize(make_v3(cam_right.xy));
            if (input->held.a)     state->player->ddp -= 100.f * normalize(make_v3(cam_right.xy));
            mesh_simulate_physics(state->player, dtime);

            active_camera->pos.x = state->player->p.x;
            active_camera->target.x = state->player->p.x;
        }
        else if (*gamemode == GAMEMODE_EDITOR)
        {
            active_camera = &state->editor_camera;
            local_persist v3 move_mask = {};
            local_persist v3 moving_start_position = {};

            if (state->selected)
            {
                if (input->pressed.g)
                {
                    move_mask = make_v3(!move_mask);
                    if (move_mask)  moving_start_position = state->selected->p;
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
                    state->selected->p = moving_start_position;
                    state->selected->transform = translation_m4(state->selected->p);
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
                    movement /= dot((state->selected->p - active_camera->pos), forward);
                    movement.x *= move_mask.x;
                    movement.y *= move_mask.y;
                    movement.z *= move_mask.z;
                    if (input->held.shift)
                        movement *= 0.1f;
                    mesh_move(state->selected, movement);
                }
            }
            else
            {
                if (input->pressed.space)
                {
                    active_camera->is_ortho = !active_camera->is_ortho;
                }
                if (input->pressed.mouse_left)
                {
                    state->selected = 0;
                    r32 least_hit_distance = 0.f;

                    Screen_To_World_Result click = screen_space_to_world(active_camera, ((r32)width/(r32)height), input->mouse.p);

                    for (Mesh *mesh = state->current_level->objects; 
                         (mesh - state->current_level->objects) < state->current_level->n_objects;
                         mesh += 1)
                    {
                        r32 hit_distance = raycast(mesh, click.pos, click.dir, active_camera->min_z, active_camera->max_z);
                        if ((hit_distance > 0) && (!least_hit_distance || (hit_distance < least_hit_distance)))
                        {
                            state->selected = mesh;
                            least_hit_distance = hit_distance;

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
            }

            editor_camera(input, &state->editor_camera, dtime);
        }
        else //if (*gamemode == GAMEMODE_MENU)
        {
        }

    }

#if WINDY_INTERNAL
    renderer->reload_shader(    state->     phong_shader, "phong");
    renderer->reload_shader(    state->      font_shader, "fonts");
    renderer->reload_shader(    state->background_shader, "background");
    renderer->reload_shader(&renderer->     debug_shader, "debug");
#endif
    renderer->set_render_targets();

    renderer->clear(CLEAR_COLOR|CLEAR_DEPTH, {0.4f, 0.7f, 0.9f}, 1.f, 1);
    renderer->set_depth_stencil(true, false, 1);

#if WINDY_INTERNAL
    {
        m4 cam_space_transform    = camera_m4(active_camera->pos, active_camera->target, active_camera->up);
        m4 screen_space_transform = perspective_m4(active_camera->fov, (r32)width/(r32)height, active_camera->min_z, active_camera->max_z);
        renderer->set_active_shader(state->background_shader);
        renderer->set_active_texture(&state->tex_sky);
        renderer->internal_sandbox_call(&cam_space_transform, &screen_space_transform);
    }
#endif

    renderer->set_active_texture(&state->tex_white);

    draw_level(renderer, state->current_level, state->phong_shader, active_camera, (r32)width/(r32)height);
    if (*gamemode == GAMEMODE_EDITOR)
    {
        if (state->selected)
        {
            renderer->draw_mesh(&state->selected->buffers, &state->selected->transform, state->phong_shader, 0, 0, 0, 0, 1);
        }
    }


    char debug_text[128] = {};
    snprintf(debug_text, 128, "FPS: %.2f", 1.f/dtime);
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
