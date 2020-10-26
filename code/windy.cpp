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
    camera->_yaw += input->dmouse.x*dtime;
    camera->_pitch -= input->dmouse.y*dtime;
    camera->_pitch  = Clamp(camera->_pitch, -PI/2.1f, PI/2.1f);
    camera->_radius -= input->dwheel*0.1f*dtime;

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

            camera->target +=  input->dmouse.y*up*dtime - input->dmouse.x*right*dtime;
            camera->pos    +=  input->dmouse.y*up*dtime - input->dmouse.x*right*dtime;
        }
        else
        {
            camera->_yaw -= input->dmouse.x*PI*dtime;
            camera->_pitch += input->dmouse.y*dtime;
            camera->_pitch  = Clamp(camera->_pitch, -PI/2.1f, PI/2.1f);
            camera->_radius -= input->dwheel*0.1f*dtime;
        }
    }
    else
    {
        v3 forward = Normalize(camera->target - camera->pos);
        camera->target += input->dwheel*dtime*forward;
        camera->pos    += input->dwheel*dtime*forward;
    }

    camera->pos.z  = Sin(camera->_pitch);
    camera->pos.x  = Cos(camera->_yaw) * Cos(camera->_pitch);
    camera->pos.y  = Sin(camera->_yaw) * Cos(camera->_pitch);
    camera->pos    = Normalize(camera->pos)*camera->_radius;
    camera->pos   += camera->target;
//    camera->target = target + make_v3(0.f, 0.f, 1.3f);
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
raycast(Platform_Mesh_Buffers *buffers, v3 from, v3 dir, r32 max_distance)
{
    r32 hit_sq = 0.f;

    v3 *verts    = (v3  *)byte_offset(buffers->wexp, buffers->wexp->vert_offset);
    u16 *indices = (u16 *)byte_offset(buffers->wexp, buffers->wexp->indices_offset);
    for (u32 i = 0; i < buffers->index_count; i += 3)
    {
        v3 *p1 = (v3 *)byte_offset(verts, WEXP_VERTEX_SIZE*(indices[i]));
        v3 *p2 = (v3 *)byte_offset(verts, WEXP_VERTEX_SIZE*(indices[i+1]));
        v3 *p3 = (v3 *)byte_offset(verts, WEXP_VERTEX_SIZE*(indices[i+2]));

        v3 n = Normalize(Cross((*p2 - *p1), (*p3 - *p1)));
        v3 u = Normalize(Cross(          n, (*p2 - *p1)));
        v3 v = Normalize(Cross( u, n));
        m4 face_local_transform = LocalSpace_m4(u, v, n, *p1);

        v3 local_start = face_local_transform * from;
        v3 local_dir   = face_local_transform *  dir;
        r32 dir_steps  = local_start.z / local_dir.z;
        v3 incident_point = local_start + dir_steps*local_dir;

        v3 incident_to_p1 = *p1 - incident_point;
        v3 incident_to_p2 = *p2 - incident_point;
        v3 incident_to_p3 = *p3 - incident_point;

        if (((Dot(incident_to_p1, incident_to_p2) < 0) ||
             (Dot(incident_to_p1, incident_to_p3) < 0)) &&
            ((Dot(incident_to_p2, incident_to_p1) < 0) ||
             (Dot(incident_to_p2, incident_to_p3) < 0)) &&
            ((Dot(incident_to_p3, incident_to_p1) < 0) ||
             (Dot(incident_to_p3, incident_to_p2) < 0)))
        {
            r32 dist = Length_Sq(local_start - incident_point);
            if (dist < max_distance*max_distance)
            {
                if (!hit_sq || (dist < hit_sq))
                    hit_sq = dist;
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
        renderer->reload_shader(state->phong_shader, renderer, "phong");
        renderer->reload_shader(state->font_shader, renderer, "fonts");

        state->obj_index_env    = load_mesh(renderer, memory->read_file, "assets/environment.wexp", &state->current_level, state->phong_shader);
        state->obj_index_player = load_mesh(renderer, memory->read_file, "assets/player.wexp",      &state->current_level, state->phong_shader);
        state->tex_white   = load_texture(renderer, &mempool, memory->read_file, "assets/blockout_white.bmp");
        state->tex_yellow  = load_texture(renderer, &mempool, memory->read_file, "assets/blockout_yellow.bmp");
        renderer->init_square_mesh(state->font_shader);

        load_font(&state->inconsolata, memory->read_file, "assets/Inconsolata.ttf", 32);

        //
        // camera set-up
        //
        state->game_camera.pos     = {0.f, -3.f, 2.f};
        state->game_camera.target  = {0.f,  0.f, 0.f};
        state->game_camera.up      = {0.f,  0.f, 1.f};
        state->game_camera._radius = 2.5f;
        state->game_camera._pitch  = 1.f;
        state->game_camera._yaw    = PI/2.f;

        state->editor_camera.pos     = {0.f, -3.f, 2.f};
        state->editor_camera.target  = {0.f,  0.f, 0.f};
        state->editor_camera.up      = {0.f,  0.f, 1.f};
        state->editor_camera._radius = 2.5f;
        state->editor_camera._pitch  = 1.f;
        state->editor_camera._yaw    = PI/2.f;
        state->editor_camera._pivot  = state->current_level.get(state->obj_index_player).p;

        //state->sun.color = {1.f,  1.f,  1.f};
        //state->sun.dir   = {0.f, -1.f, -1.f};
        state->lamp.color = {1.f,  1.f, 0.9f};
        state->lamp.p     = {0.f,  3.f, 5.f};
        memory->is_initialized = true;
    }

    //
    // ---------------------------------------------------------------
    //

    i32 last_hit = -1;
    Camera *active_camera = 0;
    { // Input Processing.
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
            v3 forward = state->game_camera.target - state->game_camera.pos;

            if (input->pressed.mouse_left)
            {
                i32 hit_index = -1;
                r32 least_hit_distance = 0.f;
                for (u32 obj_index = 0;
                     (obj_index < state->current_level.n_objects) && (hit_index < 0);
                     ++obj_index)
                {
                    r32 hit_distance = raycast(&state->current_level.get(obj_index).buffers, active_camera->pos, forward, 200.f);
                    if (!least_hit_distance || (hit_distance < least_hit_distance))
                    {
                        hit_index = obj_index;
                    }
                }

                if (hit_index > 0)  last_hit = hit_index;
            }

            editor_camera(input, &state->editor_camera, dtime);
        }
        else //if (*gamemode == GAMEMODE_MENU)
        {
        }

    }

#if WINDY_INTERNAL
    renderer->reload_shader(state->phong_shader, renderer, "phong");
    renderer->reload_shader(state->font_shader,  renderer, "fonts");
#endif
    renderer->set_render_targets();

    renderer->clear(CLEAR_COLOR|CLEAR_DEPTH, {0.06f, 0.1f, 0.15f}, 1.f, 1);
    renderer->set_depth_stencil(true, false, 1);

    renderer->set_active_texture(&state->tex_white);
    { // environment -------------------------------------------------
        Mesh *env = &state->current_level.get(state->obj_index_env);
        m4 camera = Camera_m4(active_camera->pos, active_camera->target, active_camera->up);
        m4 screen = Perspective_m4(DegToRad*60.f, (r32)width/(r32)height, 0.01f, 100.f);
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

        renderer->set_active_texture(&state->tex_yellow);
        model  = player->transform;

        renderer->draw_mesh(&player->buffers, state->phong_shader, &settings, &model, 0, 0, 0, 0);
    }

    char debug_text[128] = {};
    snprintf(debug_text, 128, "Delta mouse:\n%f\n%f\n%d", input->dmouse.x, input->dmouse.y, input->dwheel);
    renderer->draw_text(state->font_shader, &state->inconsolata, debug_text, make_v2(0, 0));
    snprintf(debug_text, 128, "FPS: %f", 1.f/dtime);
    renderer->draw_text(state->font_shader, &state->inconsolata, debug_text, make_v2(0, height-32.f));

    snprintf(debug_text, 128, "Hit: %d", last_hit);
    renderer->draw_text(state->font_shader, &state->inconsolata, debug_text, make_v2(0, height-64.f));

//    char *text = "AaBbCcDdEeFfGgHhIiJjKkLlMmNnOoPpQqRrSsTtUuVvWwXxYyZz\n0123456789 ?!\"'.,;<>[]{}()-_+=*&^%$#@/\\~`";
//    renderer->draw_text(state->font_shader, &state->inconsolata, text, make_v2(0, 0));
}
