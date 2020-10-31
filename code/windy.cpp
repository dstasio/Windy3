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

internal u32
load_mesh(Platform_Renderer *renderer, Platform_Read_File read_file, char *path, Level *level, Platform_Shader *shader = 0)
{
    Assert(level->n_objects < (MAX_LEVEL_OBJECTS - 1));

    level->last().buffers.wexp = (Wexp_Header *)read_file(path).data;
    Wexp_Header *wexp = level->last().buffers.wexp;
    assert(wexp->signature == 0x7877);
    u32 vertices_size = wexp->indices_offset - wexp->vert_offset;
    u32 indices_size  = wexp->eof_offset - wexp->indices_offset;
    level->last().buffers.index_count  = truncate_to_u16(indices_size / 2); // two bytes per index
    level->last().buffers.vert_stride  = 8*sizeof(r32);
    level->last().transform    = Identity_m4();

    renderer->load_wexp(&level->last().buffers, shader);
    return (level->n_objects)++;
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

void editor_camera(Input *input, Camera *camera, r32 dtime)
{
    if (input->held.mouse_middle)
    {
        if (input->held.shift)
        {
            v3 forward = Normalize(camera->target - camera->pos);
            v3 right   = Normalize(Cross(forward, camera->up));
            v3 up      = Normalize(Cross(right, forward));

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
        v3 forward = Normalize(camera->target - camera->pos);
        camera->target += input->mouse.wheel*dtime*forward;
        camera->pos    += input->mouse.wheel*dtime*forward;
    }

    camera->pos.z  = Sin(camera->_pitch);
    camera->pos.x  = Cos(camera->_yaw) * Cos(camera->_pitch);
    camera->pos.y  = Sin(camera->_yaw) * Cos(camera->_pitch);
    camera->pos    = Normalize(camera->pos)*camera->_radius;
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
//        ignore back-facing polygons
//
internal r32
raycast(Platform_Mesh_Buffers *buffers, v3 from, v3 dir, r32 min_distance, r32 max_distance)
{
    r32 hit_sq = 0.f;

    v3 *verts    = (v3  *)byte_offset(buffers->wexp, buffers->wexp->vert_offset);
    u16 *indices = (u16 *)byte_offset(buffers->wexp, buffers->wexp->indices_offset);
    for (u32 i = 0; i < buffers->index_count; i += 3)
    {
        v3 *p1 = ((v3 *)byte_offset(verts, WEXP_VERTEX_SIZE*(indices[i])));
        v3 *p2 = ((v3 *)byte_offset(verts, WEXP_VERTEX_SIZE*(indices[i+1])));
        v3 *p3 = ((v3 *)byte_offset(verts, WEXP_VERTEX_SIZE*(indices[i+2])));

        v3 n = Normalize(Cross(((*p2) - (*p1)), ((*p3) - (*p1))));
        if (Dot(n, dir) < 0)
        {
            v3 u = Normalize(Cross( n, ((*p2) - (*p1))));
            v3 v = Normalize(Cross( u, n));
            m4 face_local_transform = WorldToLocal_m4(u, v, n, (*p1));

            v3 local_start = face_local_transform * from;
            v3 local_dir   = NoTranslation_m4(face_local_transform) *  dir;
            r32 dir_steps  = - local_start.z / local_dir.z;
            if (dir_steps >= 0)
            {
                v3 local_incident_point = local_start + dir_steps*local_dir;

                v3 world_incident_point = ((local_incident_point.x * u) +
                                           (local_incident_point.y * v) +
                                           (local_incident_point.z * n) + (*p1));

                v3 p1_p2 = (*p2) - (*p1);
                v3 p2_p3 = (*p3) - (*p2);
                v3 p3_p1 = (*p1) - (*p3);
                v3 p1_in = world_incident_point - (*p1);
                v3 p2_in = world_incident_point - (*p2);
                v3 p3_in = world_incident_point - (*p3);
                if ((Dot(Cross(p1_p2, p1_in), n) > 0) &&
                    (Dot(Cross(p2_p3, p2_in), n) > 0) &&
                    (Dot(Cross(p3_p1, p3_in), n) > 0))
                {
                    r32 dist = Length_Sq(from - world_incident_point);

                    if ((dist > Square(min_distance)) && (dist < Square(max_distance)))
                    {
                        if (!hit_sq || (dist < hit_sq))
                        {
                            hit_sq = dist;

#if WINDY_DEBUG
                            DEBUG_buffer[DEBUG_BUFFER_new+DEBUG_BUFFER_raycast+0] = world_incident_point;
                            DEBUG_buffer[DEBUG_BUFFER_new+DEBUG_BUFFER_raycast+1] = from;
                            DEBUG_buffer[DEBUG_BUFFER_new+DEBUG_BUFFER_raycast+2] = dir;
                            DEBUG_buffer[DEBUG_BUFFER_new+DEBUG_BUFFER_raycast+3] = *p1;
                            DEBUG_buffer[DEBUG_BUFFER_new+DEBUG_BUFFER_raycast+4] = *p2;
                            DEBUG_buffer[DEBUG_BUFFER_new+DEBUG_BUFFER_raycast+5] = *p3;
#endif
                        }
                    }
                }
            }
        }
    }

    return Sqrt(hit_sq);
}

GAME_UPDATE_AND_RENDER(WindyUpdateAndRender)
{
    Game_State *state = (Game_State *)memory->storage;
    if(!memory->is_initialized)
    {
        Memory_Pool mempool = {};
        mempool.base = (u8 *)memory->storage;
        mempool.size = memory->storage_size;
        mempool.used = sizeof(Game_State);

        renderer->load_renderer(renderer);

        //
        // Shaders
        //
        // @todo: remove need for pre-allocation
        state->phong_shader = push_struct(&mempool, Platform_Shader);
        state->font_shader  = push_struct(&mempool, Platform_Shader);
        renderer->reload_shader(state->phong_shader, "phong");
        renderer->reload_shader( state->font_shader, "fonts");
        renderer->reload_shader(&renderer->debug_shader, "debug");

        load_mesh(renderer, memory->read_file, "assets/testpoly.wexp", &state->current_level, state->phong_shader);
        state->obj_index_env    = load_mesh(renderer, memory->read_file, "assets/environment.wexp", &state->current_level, state->phong_shader);
        state->obj_index_player = load_mesh(renderer, memory->read_file, "assets/player.wexp",      &state->current_level, state->phong_shader);
        state->tex_white   = load_texture(renderer, &mempool, memory->read_file, "assets/blockout_white.bmp");
        state->tex_yellow  = load_texture(renderer, &mempool, memory->read_file, "assets/blockout_yellow.bmp");
        renderer->init_square_mesh(state->font_shader);

        load_font(&state->inconsolata, memory->read_file, "assets/Inconsolata.ttf", 32);

        //
        // camera set-up
        //
        state->game_camera.pos         = {0.f, -3.f, 2.f};
        state->game_camera.target      = {0.f,  0.f, 0.f};
        state->game_camera.up          = {0.f,  0.f, 1.f};
        state->game_camera.fov         = DegToRad*60.f;
        state->game_camera.min_z       = 0.01f;
        state->game_camera.max_z       = 100.f;
        state->game_camera.is_ortho    = 0;
        state->game_camera.ortho_scale = 20.f;
        state->game_camera._radius     = 2.5f;
        state->game_camera._pitch      = 1.f;
        state->game_camera._yaw        = PI/2.f;

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
        state->editor_camera._pivot      = state->current_level.get(state->obj_index_player).p;

        //state->sun.color = {1.f,  1.f,  1.f};
        //state->sun.dir   = {0.f, -1.f, -1.f};
        state->lamp.color = {1.f,  1.f, 0.9f};
        state->lamp.p     = {0.f,  3.f, 5.f};
        memory->is_initialized = true;
    }

    //
    // ---------------------------------------------------------------
    //

    local_persist i32 last_hit = -1;
    Camera *active_camera = 0;
    { // Input Processing.
        input->mouse.dp *= global_mouse_sensitivity;
//        input->mouse.pos *= global_mouse_sensitivity;
        if (*gamemode == GAMEMODE_GAME) 
        {
            Mesh *player = &state->current_level.get(state->obj_index_player);
            active_camera = &state->game_camera;

            r32 speed = input->held.space ? 10.f : 3.f;
            v3  movement = {};
            v3  cam_forward = Normalize(state->game_camera.target - state->game_camera.pos);
            v3  cam_right   = Normalize(Cross(cam_forward, state->game_camera.up));
            if (input->held.w)     movement += make_v3(cam_forward.xy);
            if (input->held.s)     movement -= make_v3(cam_forward.xy);
            if (input->held.d)     movement += make_v3(cam_right.xy);
            if (input->held.a)     movement -= make_v3(cam_right.xy);
            if (input->held.shift) movement += state->game_camera.up;
            if (input->held.ctrl)  movement -= state->game_camera.up;
            if (movement)
            {
                player->p += Normalize(movement)*speed*dtime;
                player->transform = Translation_m4(player->p);
            }

            third_person_camera(input, &state->game_camera, player->p, dtime);
        }
        else if (*gamemode == GAMEMODE_EDITOR)
        {
            active_camera = &state->editor_camera;

            if (input->pressed.space)
            {
                active_camera->is_ortho = !active_camera->is_ortho;
            }
            if (input->pressed.mouse_left)
            {
                v3 forward = Normalize(active_camera->target - active_camera->pos);
                v3 right   = Normalize(Cross(forward, active_camera->up));
                v3 up      = Normalize(Cross(right, forward));

                i32 hit_index = -1;
                r32 least_hit_distance = 0.f;
                v3  click_p = {};
                v3  click_dir = {};
                click_dir.x =  ((input->mouse.x*2.f) - 1.f) * Tan(active_camera->fov/2.f) * ((r32)width/(r32)height) * active_camera->min_z;
                click_dir.y = -((input->mouse.y*2.f) - 1.f) * Tan(active_camera->fov/2.f) * active_camera->min_z;
                click_dir.z = active_camera->min_z;
                click_dir = click_dir.x*right + click_dir.y*up + click_dir.z*forward;
                click_p = active_camera->pos + click_dir;
                click_dir = Normalize(click_dir);

                for (u32 obj_index = 0;
                     (obj_index < state->current_level.n_objects);
                     ++obj_index)
                {
                    r32 hit_distance = raycast(&state->current_level.get(obj_index).buffers,
                                               click_p, click_dir,
                                               active_camera->min_z, active_camera->max_z);
                    if ((hit_distance > 0) && (!least_hit_distance || (hit_distance < least_hit_distance)))
                    {
                        hit_index = obj_index;
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

                last_hit = hit_index;
            }

            editor_camera(input, &state->editor_camera, dtime);
        }
        else //if (*gamemode == GAMEMODE_MENU)
        {
        }

    }

#if WINDY_INTERNAL
    renderer->reload_shader(state->phong_shader, "phong");
    renderer->reload_shader( state->font_shader, "fonts");
    renderer->reload_shader(&renderer->debug_shader, "debug");
#endif
    renderer->set_render_targets();

    renderer->clear(CLEAR_COLOR|CLEAR_DEPTH, {0.06f, 0.1f, 0.15f}, 1.f, 1);
    renderer->set_depth_stencil(true, false, 1);

    renderer->set_active_texture(&state->tex_white);
    { // environment -------------------------------------------------
        Mesh *env = &state->current_level.get(state->obj_index_env);
        m4 camera = Camera_m4(active_camera->pos, active_camera->target, active_camera->up);
        m4 screen = Perspective_m4(active_camera->fov, (r32)width/(r32)height, active_camera->min_z, active_camera->max_z);
        if (active_camera->is_ortho)
        {
            active_camera->ortho_scale = 5.f;
            active_camera->max_z = 500.f;
            screen = Ortho_m4(active_camera->ortho_scale, (r32)width/(r32)height, active_camera->min_z, active_camera->max_z);
        }

        m4 model  = env->transform;
        Platform_Phong_Settings settings = {};

        renderer->draw_mesh(&env->buffers, state->phong_shader, &settings,
                            &model, &camera, &screen,
                            (v3 *)&state->lamp, &state->game_camera.pos);
    }

    { // player ------------------------------------------------------
        Mesh *player = &state->current_level.get(state->obj_index_player);
        m4 model  = Transform_m4(state->lamp.p, make_v3(0.f), make_v3(0.1f));
        Platform_Phong_Settings settings = {};
        settings.flags |= PHONG_FLAG_SOLIDCOLOR;
        settings.flags |= PHONG_FLAG_UNLIT;
        settings.color = make_v3(1.f);

        renderer->draw_mesh(&player->buffers, state->phong_shader, &settings, &model, 0, 0, 0, 0);

        settings.flags &= ~PHONG_FLAG_UNLIT;
        settings.color = {0.8f, 0.f, 0.2f};

        model  = player->transform;
        renderer->draw_mesh(&player->buffers, state->phong_shader, &settings, &model, 0, 0, 0, 0);
    }

    { // testpoly ------------------------------------------------------
        Mesh *testpoly = &state->current_level.get(0);
        Platform_Phong_Settings settings = {};
        settings.flags |= PHONG_FLAG_SOLIDCOLOR;
        settings.color = {0.2f, 0.3f, 0.6f};

        m4 model = Identity_m4();
        renderer->draw_mesh(&testpoly->buffers, state->phong_shader, &settings, &model, 0, 0, 0, 0);
    }

    char debug_text[128] = {};
    snprintf(debug_text, 128, "Hit: %d", last_hit);
    renderer->draw_text(state->font_shader, &state->inconsolata, debug_text, make_v2(0, height-64.f));
    renderer->draw_text(state->font_shader, &state->inconsolata, "+", make_v2(width/2.f - 16.f, height/2.f - 16.f));
    snprintf(debug_text, 128, "FPS: %f", 1.f/dtime);
    renderer->draw_text(state->font_shader, &state->inconsolata, debug_text, make_v2(0, 0));

    {
        m4 camera = Camera_m4(active_camera->pos, active_camera->target, active_camera->up);
        m4 screen = Perspective_m4(active_camera->fov, (r32)width/(r32)height, active_camera->min_z, active_camera->max_z);
        if (active_camera->is_ortho)
        {
            screen = Ortho_m4(active_camera->ortho_scale, (r32)width/(r32)height, active_camera->min_z, active_camera->max_z);
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
