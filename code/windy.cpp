/* ========================================================================
   $File: $
   $Date: $
   $Revision: $
   $Creator: Davide Stasio $
   $Notice: (C) Copyright 2020 by Davide Stasio. All Rights Reserved. $
   ======================================================================== */
#include "windy.h"
#include <string.h>

internal Mesh
load_mesh(Platform_Renderer *renderer, Platform_Read_File read_file, char *path, Platform_Shader *shader = 0)
{
    Mesh mesh = {};
    mesh.buffers.wexp = (Wexp_Header *)read_file(path).data;
    Wexp_Header *wexp = mesh.buffers.wexp;
    assert(wexp->signature == 0x7877);
    u32 vertices_size = wexp->indices_offset - wexp->vert_offset;
    u32 indices_size  = wexp->eof_offset - wexp->indices_offset;
    mesh.buffers.index_count  = truncate_to_u16(indices_size / 2); // two bytes per index
    mesh.buffers.vert_stride  = 8*sizeof(r32);
    mesh.transform    = Identity_m4();

    renderer->load_wexp(renderer, &mesh.buffers, shader);
    return mesh;
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
    renderer->init_texture(renderer, &texture);
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
        

        state->environment = load_mesh(renderer, memory->read_file, "assets/environment.wexp", state->phong_shader);
        state->player      = load_mesh(renderer, memory->read_file, "assets/player.wexp",      state->phong_shader);
        state->tex_white   = load_texture(renderer, &mempool, memory->read_file, "assets/blockout_white.bmp");
        state->tex_yellow  = load_texture(renderer, &mempool, memory->read_file, "assets/blockout_yellow.bmp");
        renderer->init_square_mesh(renderer, state->font_shader);

        load_font(&state->inconsolata, memory->read_file, "assets/Inconsolata.ttf", 32);

        //
        // camera set-up
        //
        state->main_cam.pos    = {0.f, -3.f, 2.f};
        state->main_cam.target = {0.f,  0.f, 0.f};
        state->main_cam.up     = {0.f,  0.f, 1.f};
        state->cam_radius = 11.f;
        state->cam_vtheta = 0.5f;
        state->cam_htheta = -PI/2.f;

        //state->sun.color = {1.f,  1.f,  1.f};
        //state->sun.dir   = {0.f, -1.f, -1.f};
        state->lamp.color = {1.f,  1.f, 0.9f};
        state->lamp.p     = {0.f,  0.f, 5.f};
        memory->is_initialized = true;
    }

    //
    // ---------------------------------------------------------------
    //

    { // Input Processing.
        r32 speed = input->held.space ? 10.f : 3.f;
        v3  movement = {};
        v3  cam_forward = Normalize(state->main_cam.target - state->main_cam.pos);
        v3  cam_right   = Normalize(Cross(cam_forward, state->main_cam.up));
        if (input->held.w)     movement += make_v3(cam_forward.xy);
        if (input->held.s)     movement -= make_v3(cam_forward.xy);
        if (input->held.d)     movement += make_v3(cam_right.xy);
        if (input->held.a)     movement -= make_v3(cam_right.xy);
        if (input->held.shift) movement += state->main_cam.up;
        if (input->held.ctrl)  movement -= state->main_cam.up;
        if (movement)
        {
            state->player.p += Normalize(movement)*speed*dtime;
            state->player.transform = Translation_m4(state->player.p);
        }
        state->cam_htheta += input->dmouse.x*dtime;
        state->cam_vtheta -= input->dmouse.y*dtime;
        state->cam_vtheta = Clamp(state->cam_vtheta, -PI/2.1f, PI/2.1f);
        state->cam_radius -= input->dwheel*dtime;

        state->main_cam.pos.z  = Sin(state->cam_vtheta);
        state->main_cam.pos.x  = Cos(state->cam_htheta) * Cos(state->cam_vtheta);
        state->main_cam.pos.y  = Sin(state->cam_htheta) * Cos(state->cam_vtheta);
        state->main_cam.pos    = Normalize(state->main_cam.pos)*state->cam_radius;
        state->main_cam.pos   += state->player.p;
        state->main_cam.target = state->player.p + make_v3(0.f, 0.f, 1.3f);
    }

#if WINDY_INTERNAL
    renderer->reload_shader(state->phong_shader, renderer, "phong");
    renderer->reload_shader(state->font_shader,  renderer, "fonts");
#endif
    renderer->set_render_targets(renderer);

    renderer->clear(renderer, CLEAR_COLOR|CLEAR_DEPTH, {0.06f, 0.5f, 0.8f}, 1.f, 1);
    renderer->set_depth_stencil(renderer, true, false, 1);

    renderer->set_active_texture(renderer, &state->tex_white);
    { // environment -------------------------------------------------
        m4 camera = Camera_m4(state->main_cam.pos, state->main_cam.target, state->main_cam.up);
        m4 screen = Perspective_m4(DegToRad*60.f, (r32)WIDTH/(r32)HEIGHT, 0.01f, 100.f);
        m4 model  = state->environment.transform;

        renderer->draw_mesh(renderer, &state->environment.buffers, state->phong_shader,
                            &model, &camera, &screen,
                            (v3 *)&state->lamp, &state->main_cam.pos);
    }

    { // player ------------------------------------------------------
        m4 model  = Transform_m4(state->lamp.p, make_v3(0.f), make_v3(0.1f));
        renderer->draw_mesh(renderer, &state->player.buffers, state->phong_shader, &model, 0, 0, 0, 0);

        renderer->set_active_texture(renderer, &state->tex_yellow);
        model  = state->player.transform;

        renderer->draw_mesh(renderer, &state->player.buffers, state->phong_shader, &model, 0, 0, 0, 0);
    }

    char *text = "AaBbCcDdEeFfGgHhIiJjKkLlMmNnOoPpQqRrSsTtUuVvWwXxYyZz\n0123456789 ?!\"'.,;<>[]{}()-_+=*&^%$#@/\\~`";
    renderer->draw_text(renderer, state->font_shader, &state->inconsolata, text, make_v2(0, 0));
}
