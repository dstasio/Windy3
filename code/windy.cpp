/* ========================================================================
   $File: $
   $Date: $
   $Revision: $
   $Creator: Davide Stasio $
   $Notice: (C) Copyright 2020 by Davide Stasio. All Rights Reserved. $
   ======================================================================== */
#include <d3d11.h>
#include "windy.h"
#include <string.h>

#define byte_offset(base, offset) ((u8*)(base) + (offset))

inline u16 truncate_to_u16(u32 v) {assert(v <= 0xFFFF); return (u16)v; };

inline void
cat(char *src0, char *src1, char *dest)
{
    while (*src0)  *(dest++) = *(src0++);
    while (*src1)  *(dest++) = *(src1++);
    *dest = '\0';
}

Mesh make_square_mesh(ID3D11Device *dev, Shader_Pack *shader)
{
    Mesh mesh = {};
    r32 square[] = {
        0.f, -1.f,  0.f, 1.f,
        1.f, -1.f,  1.f, 1.f,
        1.f,  0.f,  1.f, 0.f,

        1.f,  0.f,  1.f, 0.f,
        0.f,  0.f,  0.f, 0.f,
        0.f, -1.f,  0.f, 1.f
    };

    u32 vertices_size = sizeof(square);
    mesh.vert_stride  = 4*sizeof(r32);
    mesh.transform    = Identity_m4();

    //
    // vertex buffer
    //
    D3D11_SUBRESOURCE_DATA raw_vert_data = {square};
    D3D11_BUFFER_DESC vert_buff_desc     = {};
    vert_buff_desc.ByteWidth             = sizeof(square);
    vert_buff_desc.Usage                 = D3D11_USAGE_IMMUTABLE;
    vert_buff_desc.BindFlags             = D3D11_BIND_VERTEX_BUFFER;
    vert_buff_desc.StructureByteStride   = mesh.vert_stride;
    dev->CreateBuffer(&vert_buff_desc, &raw_vert_data, &mesh.vbuff);

    //
    // input layout description
    //
    D3D11_INPUT_ELEMENT_DESC in_desc[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };
    dev->CreateInputLayout(in_desc, 2, shader->vertex_file.data, shader->vertex_file.size, &mesh.in_layout);

    return mesh;
}

internal Mesh
load_wexp(ID3D11Device *dev, Platform_Read_File *read_file, char *path, Shader_Pack *shader)
{
    Mesh mesh = {};
    Wexp_Header *wexp = (Wexp_Header *)read_file(path).data;
    assert(wexp->signature == 0x7877);
    u32 vertices_size = wexp->indices_offset - wexp->vert_offset;
    u32 indices_size  = wexp->eof_offset - wexp->indices_offset;
    mesh.index_count  = truncate_to_u16(indices_size / 2); // two bytes per index
    mesh.vert_stride  = 8*sizeof(r32);
    mesh.transform    = Identity_m4();

    //
    // vertex buffer
    //
    D3D11_SUBRESOURCE_DATA raw_vert_data = {byte_offset(wexp, wexp->vert_offset)};
    D3D11_BUFFER_DESC vert_buff_desc     = {};
    vert_buff_desc.ByteWidth             = vertices_size;
    vert_buff_desc.Usage                 = D3D11_USAGE_IMMUTABLE;
    vert_buff_desc.BindFlags             = D3D11_BIND_VERTEX_BUFFER;
    vert_buff_desc.StructureByteStride   = mesh.vert_stride;
    dev->CreateBuffer(&vert_buff_desc, &raw_vert_data, &mesh.vbuff);

    //
    // input layout description
    //
    D3D11_INPUT_ELEMENT_DESC in_desc[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0}
    };
    dev->CreateInputLayout(in_desc, 3, shader->vertex_file.data, shader->vertex_file.size, &mesh.in_layout);

    //
    // index buffer
    //
    D3D11_SUBRESOURCE_DATA index_data = {byte_offset(wexp, wexp->indices_offset)};
    D3D11_BUFFER_DESC index_buff_desc = {};
    index_buff_desc.ByteWidth         = indices_size;
    index_buff_desc.Usage             = D3D11_USAGE_IMMUTABLE;
    index_buff_desc.BindFlags         = D3D11_BIND_INDEX_BUFFER;
    dev->CreateBuffer(&index_buff_desc, &index_data, &mesh.ibuff);

    return mesh;
}

internal Texture
load_bitmap(Memory_Pool *mempool, Platform_Read_File *read_file, char *path)
{
    Texture texture = {};
    Bitmap_Header *bmp = (Bitmap_Header *)read_file(path).data;
    texture.width  = bmp->Width;
    texture.height = bmp->Height;
    texture.size = texture.width*texture.height*4;
    texture.data = push_array(mempool, texture.size, u8);

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
            u32 *dest_pixel = (u32 *)byte_offset(texture.data, dest_offset);
            *dest_pixel = (r_mask & (*src_pixel >> 16)) | (g_mask & (*src_pixel)) | (b_mask & (*src_pixel >> 8)) | a_mask;
        }
    }

    return texture;
}

internal Texture
load_texture(ID3D11Device *dev, ID3D11DeviceContext *context, Memory_Pool *mempool, Platform_Read_File *read_file, char *path)
{
    Texture texture = load_bitmap(mempool, read_file, path);
    D3D11_TEXTURE2D_DESC tex_desc = {};
    tex_desc.Width              = texture.width;
    tex_desc.Height             = texture.height;
    tex_desc.MipLevels          = 0;
    tex_desc.ArraySize          = 1;
    tex_desc.Format             = DXGI_FORMAT_R8G8B8A8_UNORM;
    tex_desc.SampleDesc.Count   = 1;
    tex_desc.SampleDesc.Quality = 0;
    tex_desc.Usage              = D3D11_USAGE_DEFAULT;
    tex_desc.BindFlags          = D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_RENDER_TARGET;
    tex_desc.MiscFlags          = D3D11_RESOURCE_MISC_GENERATE_MIPS;

    dev->CreateTexture2D(&tex_desc, 0, &texture.handle);
    dev->CreateShaderResourceView(texture.handle, 0, &texture.view);
    context->UpdateSubresource(texture.handle, 0, 0, texture.data, texture.width*4, 0);
    context->GenerateMips(texture.view);
    
    return texture;
}

inline Font *
load_font(Font *font, Platform_Read_File *read_file, char *path, r32 height)
{
    Input_File font_file = read_file(path);
    stbtt_InitFont(&font->info, font_file.data, stbtt_GetFontOffsetForIndex(font_file.data, 0));
    font->height = height;
    font->scale = stbtt_ScaleForPixelHeight(&font->info, height);
    return font;
}

internal Shader_Pack *
reload_shader(Shader_Pack *shader, ID3D11Device *dev, char *name, Platform_Reload_Changed_File *reload_if_changed)
{
    char vertex_path[MAX_PATH] = {};
    char  pixel_path[MAX_PATH] = {};
    cat("assets\\",    name, vertex_path);
    cat(vertex_path, ".vsh", vertex_path);
    cat("assets\\",    name,  pixel_path);
    cat( pixel_path, ".psh",  pixel_path);
    shader->vertex_file.path = vertex_path;
    shader->pixel_file.path  =  pixel_path;

    if(reload_if_changed(&shader->vertex_file))
        dev->CreateVertexShader(shader->vertex_file.data, shader->vertex_file.size, 0, &shader->vertex);

    if(reload_if_changed(&shader->pixel_file))
        dev->CreatePixelShader(shader->pixel_file.data, shader->pixel_file.size, 0, &shader->pixel);

    return shader;
}

inline void
set_active_mesh(ID3D11DeviceContext *context, Mesh *mesh)
{
    u32 offsets = 0;
    u32 stride = mesh->vert_stride;
    context->IASetVertexBuffers(0, 1, &mesh->vbuff, &stride, &offsets);
    context->IASetInputLayout(mesh->in_layout);
    if(mesh->ibuff)  context->IASetIndexBuffer(mesh->ibuff, DXGI_FORMAT_R16_UINT, 0);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

inline void
set_active_texture(ID3D11DeviceContext *context, Texture *texture)
{
    context->PSSetShaderResources(0, 1, &texture->view); 
}

inline void
set_active_shader(ID3D11DeviceContext *context, Shader_Pack *shader)
{
    context->VSSetShader(shader->vertex, 0, 0);
    context->PSSetShader(shader->pixel,  0, 0);
}

// (0,0) = Top-Left; (WIDTH,HEIGHT) = Bottom-Right
// @todo: test sub-pixel placement with AA.
inline void
draw_rect(ID3D11DeviceContext *context, Shader_Pack *shader, Game_State *state, v2 size, v2 pos)
{
    D3D11_MAPPED_SUBRESOURCE matrices_map = {};
    context->Map(state->matrix_buff, 0, D3D11_MAP_WRITE_DISCARD, 0, &matrices_map);

    m4 *matrix_buffer = (m4 *)matrices_map.pData;

    size.x /= (r32)WIDTH;
    size.y /= (r32)HEIGHT;
    pos.x  /= (r32)WIDTH;
    pos.y  /= (r32)HEIGHT;
    pos.x   =  (pos.x*2.f - 1.f);
    pos.y   = -(pos.y*2.f - 1.f);

    matrix_buffer[0] = Translation_m4(pos.x, pos.y, 0)*Scale_m4(size*2.f);
    context->Unmap(state->matrix_buff, 0);

    context->OMSetDepthStencilState(state->nodepth_nostencil_state, 1);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    set_active_mesh(context, &state->square);
    context->Draw(6, 0);
}

internal r32
draw_char(ID3D11Device *dev, ID3D11DeviceContext *context, Game_State *state, Font *font, char character, v2 pos)
{
    set_active_shader(context, state->font_shader);

    Assert(character >= first_nonwhite_char);
    Assert(character <=  last_nonwhite_char);
    Texture *texture = &font->chars[character - first_nonwhite_char];
    if (!texture->view)
    {
        // Loading character bitmap
        u8 *font_bitmap = stbtt_GetCodepointBitmap(&font->info, 0, stbtt_ScaleForPixelHeight(&font->info, font->height),
                                                   character, (i32 *)&texture->width, (i32 *)&texture->height, 0, 0);
        Assert(texture->width  < 0x80000000);
        Assert(texture->height < 0x80000000);
        texture->size = texture->width*texture->height*4;
        Assert(texture->size/4 < 2500);
        u32 texture_data[2500] = {};

        for(u32 y = 0; y < texture->height; ++y) {
            for(u32 x = 0; x < texture->width; ++x) {
                u32 src = font_bitmap[y*texture->width + x];
                texture_data[y*texture->width + x] = ((src << 24) | (src << 16) | (src << 8) | src);
            }
        }
        stbtt_FreeBitmap(font_bitmap, font->info.userdata);

        D3D11_TEXTURE2D_DESC tex_desc = {};
        tex_desc.Width              = texture->width;
        tex_desc.Height             = texture->height;
        tex_desc.MipLevels          = 1;
        tex_desc.ArraySize          = 1;
        tex_desc.Format             = DXGI_FORMAT_R8G8B8A8_UNORM;
        tex_desc.SampleDesc.Count   = 1;
        tex_desc.SampleDesc.Quality = 0;
        tex_desc.Usage              = D3D11_USAGE_IMMUTABLE;
        tex_desc.BindFlags          = D3D11_BIND_SHADER_RESOURCE;

        D3D11_SUBRESOURCE_DATA subres = {texture_data, texture->width*4};
        dev->CreateTexture2D(&tex_desc, &subres, &texture->handle);
        dev->CreateShaderResourceView(texture->handle, 0, &texture->view);
    }

    set_active_texture(context, texture);
    v2 size = {(r32)texture->width, (r32)texture->height};
    //size *= 32.f/(r32)texture->height;
    draw_rect(context, state->font_shader, state, size, pos);
    return size.x;
}

inline void
draw_text(ID3D11Device *dev, ID3D11DeviceContext *context, Game_State *state, Font *font, char *text, v2 pivot)
{
    i32 ascent, descent, line_gap;
    stbtt_GetFontVMetrics(&font->info, &ascent, &descent, &line_gap);
    ascent   = (i32)((r32)ascent   * font->scale);
    descent  = (i32)((r32)descent  * font->scale);
    line_gap = (i32)((r32)line_gap * font->scale);
    v2 pos = pivot;
    i32 min_x, min_y, max_x, max_y;
    stbtt_GetFontBoundingBox(&font->info, &min_x, &min_y, &max_x, &max_y);
    v2 font_min = make_v2((r32)min_x, (r32)min_y)*font->scale;
    v2 font_max = make_v2((r32)max_x, (r32)max_y)*font->scale;
    for(char *c = text; *c != '\0'; ++c) {
        if (*c == '\n') {
            pos = pivot;
            pos.y += ascent - descent + line_gap;
        }
        else if (*c == ' ') {
            pos.x += font_max.x;
        }
        else {
            stbtt_GetCodepointBox(&font->info, *c, &min_x, &min_y, &max_x, &max_y);
            v2 min = make_v2((r32)min_x, (r32)min_y)*font->scale;
            v2 max = make_v2((r32)max_x, (r32)max_y)*font->scale;
            draw_char(dev, context, state, font, *c,
                      make_v2(pos.x + min.x - font_min.x, pos.y + font_max.y - max.y));
            pos.x += font_max.x;
        }
    }
}

struct Light_Buffer
{
    v3 color;
    r32 pad0_; // 16 bytes
    v3 p;      // 28 bytes
    r32 pad1_;
    v3 eye;    // 40 bytes
};

GAME_UPDATE_AND_RENDER(WindyUpdateAndRender)
{
    Game_State *state = (Game_State *)memory->storage;
    if(!memory->is_initialized)
    {
        Memory_Pool mempool = {};
        mempool.base = (u8 *)memory->storage;
        mempool.size = memory->storage_size;
        mempool.used = sizeof(Game_State);

        D3D11_VIEWPORT Viewport = {};
        Viewport.TopLeftX = 0;
        Viewport.TopLeftY = 0;
        Viewport.Width = WIDTH;
        Viewport.Height = HEIGHT;
        Viewport.MinDepth = 0.f;
        Viewport.MaxDepth = 1.f;
        context->RSSetViewports(1, &Viewport);

        //
        // Shaders
        //
        // @todo: remove need for pre-allocation
        state->phong_shader = push_struct(&mempool, Shader_Pack);
        state->font_shader  = push_struct(&mempool, Shader_Pack);
        reload_shader(state->phong_shader, device, "phong", memory->reload_if_changed);
        reload_shader(state->font_shader,  device, "fonts", memory->reload_if_changed);
        
        //
        // allocating rgb and depth buffers
        //
        D3D11_TEXTURE2D_DESC depth_buffer_desc = {};
        depth_buffer_desc.Width = WIDTH;
        depth_buffer_desc.Height = HEIGHT;
        depth_buffer_desc.MipLevels = 1;
        depth_buffer_desc.ArraySize = 1;
        depth_buffer_desc.Format = DXGI_FORMAT_D32_FLOAT;
        depth_buffer_desc.SampleDesc.Count = 1;
        depth_buffer_desc.SampleDesc.Quality = 0;
        depth_buffer_desc.Usage = D3D11_USAGE_DEFAULT;
        depth_buffer_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
        depth_buffer_desc.MiscFlags = 0;

        ID3D11Texture2D *depth_texture;
        device->CreateTexture2D(&depth_buffer_desc, 0, &depth_texture);

        { // Depth states.
            D3D11_DEPTH_STENCIL_VIEW_DESC depth_view_desc = {DXGI_FORMAT_D32_FLOAT, D3D11_DSV_DIMENSION_TEXTURE2D};
            device->CreateRenderTargetView(rendering_backbuffer, 0, &state->render_target_rgb);
            device->CreateDepthStencilView(depth_texture, &depth_view_desc, &state->render_target_depth);

            D3D11_DEPTH_STENCIL_DESC depth_stencil_settings;
            depth_stencil_settings.DepthEnable    = 1;
            depth_stencil_settings.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
            depth_stencil_settings.DepthFunc      = D3D11_COMPARISON_LESS;
            depth_stencil_settings.StencilEnable  = 0;
            depth_stencil_settings.StencilReadMask;
            depth_stencil_settings.StencilWriteMask;
            depth_stencil_settings.FrontFace      = {D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_COMPARISON_ALWAYS};
            depth_stencil_settings.BackFace       = {D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_COMPARISON_ALWAYS};

            device->CreateDepthStencilState(&depth_stencil_settings, &state->depth_nostencil_state);

            depth_stencil_settings.DepthEnable    = 0;
            depth_stencil_settings.DepthFunc      = D3D11_COMPARISON_ALWAYS;

            device->CreateDepthStencilState(&depth_stencil_settings, &state->nodepth_nostencil_state);
        }

        ID3D11SamplerState *sampler;
        D3D11_SAMPLER_DESC sampler_desc = {};
        sampler_desc.Filter = D3D11_FILTER_ANISOTROPIC;//D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
        sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
        sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
        sampler_desc.MipLODBias = -1;
        sampler_desc.MaxAnisotropy = 16;
        sampler_desc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
        sampler_desc.MinLOD = 0;
        sampler_desc.MaxLOD = 100;
        device->CreateSamplerState(&sampler_desc, &sampler);
        context->PSSetSamplers(0, 1, &sampler);

        //
        // constant buffer setup
        //
        D3D11_BUFFER_DESC MatrixBufferDesc = {};
        MatrixBufferDesc.ByteWidth = 3*sizeof(m4);
        MatrixBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
        MatrixBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        MatrixBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        MatrixBufferDesc.MiscFlags = 0;
        MatrixBufferDesc.StructureByteStride = sizeof(m4);
        device->CreateBuffer(&MatrixBufferDesc, 0, &state->matrix_buff);
        context->VSSetConstantBuffers(0, 1, &state->matrix_buff);

        D3D11_BUFFER_DESC light_buff_desc = {};
        light_buff_desc.ByteWidth = 16*3;
        light_buff_desc.Usage = D3D11_USAGE_DYNAMIC;
        light_buff_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        light_buff_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        light_buff_desc.MiscFlags = 0;
        light_buff_desc.StructureByteStride = sizeof(v3);
        device->CreateBuffer(&light_buff_desc, 0, &state->light_buff);
        context->PSSetConstantBuffers(0, 1, &state->light_buff);

        //
        // rasterizer set-up
        //
        D3D11_RASTERIZER_DESC raster_settings = {};
        raster_settings.FillMode = D3D11_FILL_SOLID;
        raster_settings.CullMode = D3D11_CULL_BACK;
        raster_settings.FrontCounterClockwise = 1;
        raster_settings.DepthBias = 0;
        raster_settings.DepthBiasClamp = 0;
        raster_settings.SlopeScaledDepthBias = 0;
        raster_settings.DepthClipEnable = 1;
        raster_settings.ScissorEnable = 0;
        raster_settings.MultisampleEnable = 0;
        raster_settings.AntialiasedLineEnable = 0;

        ID3D11RasterizerState *raster_state = 0;
        device->CreateRasterizerState(&raster_settings, &raster_state);
        context->RSSetState(raster_state);

        state->environment = load_wexp(device, memory->read_file, "assets/environment.wexp", state->phong_shader);
        state->player      = load_wexp(device, memory->read_file, "assets/sphere.wexp",      state->phong_shader);
        state->tex_white   = load_texture(device, context, &mempool, memory->read_file, "assets/blockout_white.bmp");
        state->tex_yellow  = load_texture(device, context, &mempool, memory->read_file, "assets/blockout_yellow.bmp");
        state->square      = make_square_mesh(device, state->font_shader);

        load_font(&state->inconsolata, memory->read_file, "assets/Inconsolata.ttf", 32);

        // 
        // loading font
        //

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
        state->main_cam.target = state->player.p;
    }

#if WINDY_INTERNAL
    reload_shader(state->phong_shader, device, "phong", memory->reload_if_changed);
    reload_shader(state->font_shader,  device, "fonts", memory->reload_if_changed);
#endif
    set_active_shader(context, state->phong_shader);
    context->OMSetRenderTargets(1, &state->render_target_rgb, state->render_target_depth);

    r32 ClearColor[] = {0.06f, 0.5f, 0.8f, 1.f};
    context->ClearRenderTargetView(state->render_target_rgb, ClearColor);
    context->ClearDepthStencilView(state->render_target_depth, D3D11_CLEAR_DEPTH, 1.f, 1);


    set_active_mesh(context, &state->environment);
    set_active_texture(context, &state->tex_white);
    //set_active_shader(context, phong_shader);

    context->OMSetDepthStencilState(state->depth_nostencil_state, 1);
    { // environment -------------------------------------------------
        D3D11_MAPPED_SUBRESOURCE matrices_map = {};
        D3D11_MAPPED_SUBRESOURCE lights_map = {};
        context->Map(state->light_buff, 0, D3D11_MAP_WRITE_DISCARD, 0, &lights_map);
        Light_Buffer *lights_mapped = (Light_Buffer *)lights_map.pData;
        lights_mapped->color = state->lamp.color;
        lights_mapped->p     = state->lamp.p;
        lights_mapped->eye   = state->main_cam.pos;
        context->Unmap(state->light_buff, 0);

        context->Map(state->matrix_buff, 0, D3D11_MAP_WRITE_DISCARD, 0, &matrices_map);
        m4 *matrix_buffer = (m4 *)matrices_map.pData;
        matrix_buffer[0] = state->environment.transform;
        matrix_buffer[1] = Camera_m4(state->main_cam.pos, state->main_cam.target, state->main_cam.up);
        matrix_buffer[2] = Perspective_m4(DegToRad*60.f, (r32)WIDTH/(r32)HEIGHT, 0.01f, 100.f);
        context->Unmap(state->matrix_buff, 0);

        context->DrawIndexed(204, 0, 0);
    }

    { // player ------------------------------------------------------
        set_active_mesh(context, &state->player);
        D3D11_MAPPED_SUBRESOURCE matrices_map = {};
        D3D11_MAPPED_SUBRESOURCE lights_map = {};

        context->Map(state->matrix_buff, 0, D3D11_MAP_WRITE_DISCARD, 0, &matrices_map);
        m4 *matrix_buffer = (m4 *)matrices_map.pData;
        matrix_buffer[0] = Transform_m4(state->lamp.p, make_v3(0.f), make_v3(0.1f));
        matrix_buffer[1] = Camera_m4(state->main_cam.pos, state->main_cam.target, state->main_cam.up);
        matrix_buffer[2] = Perspective_m4(DegToRad*60.f, (r32)WIDTH/(r32)HEIGHT, 0.01f, 100.f);
        context->Unmap(state->matrix_buff, 0);

        context->DrawIndexed(2880, 0, 0);

        set_active_texture(context, &state->tex_yellow);

        context->Map(state->light_buff, 0, D3D11_MAP_WRITE_DISCARD, 0, &lights_map);
        Light_Buffer *lights_mapped = (Light_Buffer *)lights_map.pData;
        lights_mapped->color = state->lamp.color;
        lights_mapped->p     = state->lamp.p-state->player.p;
        lights_mapped->eye   = state->main_cam.pos;
        context->Unmap(state->light_buff, 0);

        context->Map(state->matrix_buff, 0, D3D11_MAP_WRITE_DISCARD, 0, &matrices_map);
        matrix_buffer = (m4 *)matrices_map.pData;
        matrix_buffer[0] = Translation_m4(state->player.p);
        matrix_buffer[1] = Camera_m4(state->main_cam.pos, state->main_cam.target, state->main_cam.up);
        matrix_buffer[2] = Perspective_m4(DegToRad*60.f, (r32)WIDTH/(r32)HEIGHT, 0.01f, 100.f);
        context->Unmap(state->matrix_buff, 0);

        context->DrawIndexed(2880, 0, 0);
    }

    char *text = "AaBbCcDdEeFfGgHhIiJjKkLlMmNnOoPpQqRrSsTtUuVvWwXxYyZz\n0123456789 ?!\"'.,;<>[]{}()-_+=*&^%$#@/\\~`";
    draw_text(device, context, state, &state->inconsolata, text, make_v2(0, 0));
}
