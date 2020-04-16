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

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#define byte_offset(base, offset) ((u8*)(base) + (offset))

inline u16 truncate_to_u16(u32 v) {assert(v <= 0xFFFF); return (u16)v; };

inline void
cat(char *src0, char *src1, char *dest)
{
    while (*src0)  *(dest++) = *(src0++);
    while (*src1)  *(dest++) = *(src1++);
    *dest = '\0';
}

//{
//    Mesh_Data mesh = {};
//    u32 vertices_size = wexp->indices_offset - wexp->vert_offset;
//    u32 indices_size  = wexp->eof_offset - wexp->indices_offset;
//    mesh.index_count  = truncate_to_u16(indices_size / 2); // two bytes per index
//    mesh.vert_stride  = 8*sizeof(r32);
//    mesh.transform    = Identity_m4();
//
//    r32 square = {
//        -1.f, -1.f,  0.f, 1.f,
//         1.f, -1.f,  1.f, 1.f,
//         1.f,  1.f,  1.f, 0.f,
//        -1.f,  1.f,  0.f, 0.f
//    };
//
//    //
//    // vertex buffer
//    //
//    D3D11_SUBRESOURCE_DATA raw_vert_data = {square};
//    D3D11_BUFFER_DESC vert_buff_desc     = {};
//    vert_buff_desc.ByteWidth             = sizeof(square);
//    vert_buff_desc.Usage                 = D3D11_USAGE_IMMUTABLE;
//    vert_buff_desc.BindFlags             = D3D11_BIND_VERTEX_BUFFER;
//    vert_buff_desc.StructureByteStride   = 4*sizeof(r32);
//    dev->CreateBuffer(&vert_buff_desc, &raw_vert_data, &mesh.vbuff);
//
//    //
//    // input layout description
//    //
//    D3D11_INPUT_ELEMENT_DESC in_desc[] = {
//        {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0},
//        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
//    };
//    dev->CreateInputLayout(in_desc, 2, vshader.data, vshader.size, &mesh.in_layout);
//
//    return mesh;
//}

internal Mesh_Data
load_wexp(ID3D11Device *dev, Platform_Read_File *read_file, char *path, Input_File vshader)
{
    Mesh_Data mesh = {};
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
    dev->CreateInputLayout(in_desc, 3, vshader.data, vshader.size, &mesh.in_layout);

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

inline void
set_active_mesh(ID3D11DeviceContext *context, Mesh_Data *mesh)
{
    u32 offsets = 0;
    u32 stride = mesh->vert_stride;
    context->IASetVertexBuffers(0, 1, &mesh->vbuff, &stride, &offsets);
    context->IASetInputLayout(mesh->in_layout);
    context->IASetIndexBuffer(mesh->ibuff, DXGI_FORMAT_R16_UINT, 0);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

internal Image_Data
load_bitmap(Memory_Pool *mempool, Platform_Read_File *read_file, char *path)
{
    Image_Data image = {};
    Bitmap_Header *bmp = (Bitmap_Header *)read_file(path).data;
    image.width  = bmp->Width;
    image.height = bmp->Height;
    image.size = image.width*image.height*4;
    image.data = push_array(mempool, image.size, u8);

    assert(bmp->BitsPerPixel == 24);
    assert(bmp->Compression == 0);

    u32 scanline_byte_count = image.width*3;
    scanline_byte_count    += (4 - (scanline_byte_count & 0x3)) & 0x3;
    u32 r_mask = 0x000000FF;
    u32 g_mask = 0x0000FF00;
    u32 b_mask = 0x00FF0000;
    u32 a_mask = 0xFF000000;
    for(u32 y = 0; y < image.height; ++y)
    {
        for(u32 x = 0; x < image.width; ++x)
        {
            u32 src_offset  = (image.height - y - 1)*scanline_byte_count + x*3;
            u32 dest_offset = y*image.width*4 + x*4;
            u32 *src_pixel  = (u32 *)byte_offset(bmp, bmp->DataOffset + src_offset);
            u32 *dest_pixel = (u32 *)byte_offset(image.data, dest_offset);
            *dest_pixel = (r_mask & (*src_pixel >> 16)) | (g_mask & (*src_pixel)) | (b_mask & (*src_pixel >> 8)) | a_mask;
        }
    }

    return image;
}

internal Texture_Data
load_texture(ID3D11Device *dev, ID3D11DeviceContext *context, Memory_Pool *mempool, Platform_Read_File *read_file, char *path)
{
    // Loading font
    //Image_Data image = {};
    //Input_File inconsolata_file = read_file("assets/Inconsolata.ttf");
    //stbtt_fontinfo inconsolata_info;
    //stbtt_InitFont(&inconsolata_info, inconsolata_file.data, stbtt_GetFontOffsetForIndex(inconsolata_file.data, 0));
    //u8 *font_bitmap = stbtt_GetCodepointBitmap(&inconsolata_info, 0, stbtt_ScaleForPixelHeight(&inconsolata_info, 50),
    //                                           'N', (i32 *)&image.width, (i32 *)&image.height, 0, 0);
    //image.size = image.width*image.height*4;
    //image.data = PushArray(mempool, image.size, u32);

    //for(u32 y = 0; y < image.height; ++y) {
    //    for(u32 x = 0; x < image.width; ++x) {
    //        u32 src = font_bitmap[y*image.width + x];
    //        ((u32 *)image.data)[y*image.width + x] = ((src << 24) | (src << 16) | (src << 8) | src);
    //    }
    //}

    Texture_Data texture = {};
    Image_Data image = load_bitmap(mempool, read_file, path);
    D3D11_TEXTURE2D_DESC tex_desc = {};
    tex_desc.Width = image.width;
    tex_desc.Height = image.height;
    tex_desc.MipLevels = 0;
    tex_desc.ArraySize = 1;
    tex_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    tex_desc.SampleDesc.Count = 1;
    tex_desc.SampleDesc.Quality = 0;
    tex_desc.Usage = D3D11_USAGE_DEFAULT;
    tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_RENDER_TARGET;
    tex_desc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;

    dev->CreateTexture2D(&tex_desc, 0, &texture.handle);
    dev->CreateShaderResourceView(texture.handle, 0, &texture.view);
    context->UpdateSubresource(texture.handle, 0, 0, image.data, image.width*4, 0);
    context->GenerateMips(texture.view);
    
    return texture;
}

internal Shader_Pack *
reload_shader(Shader_Pack *shader, Platform_Reload_Changed_File *reload_if_changed, ID3D11Device *dev, char *name)
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
set_active_texture(ID3D11DeviceContext *context, Texture_Data *texture)
{
    context->PSSetShaderResources(0, 1, &texture->view); 
}

inline void
set_active_shader(ID3D11DeviceContext *context, Shader_Pack *shader)
{
    context->VSSetShader(shader->vertex, 0, 0);
    context->PSSetShader(shader->pixel,  0, 0);
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
        state->phong_shader = push_struct(&mempool, Shader_Pack);
#if !WINDY_INTERNAL
        reload_shader(state->phong_shader, memory->reload_if_changed, device, "phong");
#endif
        
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

        D3D11_DEPTH_STENCIL_VIEW_DESC depth_view_desc = {DXGI_FORMAT_D32_FLOAT, D3D11_DSV_DIMENSION_TEXTURE2D};
        device->CreateRenderTargetView(rendering_backbuffer, 0, &state->render_target_rgb);
        device->CreateDepthStencilView(depth_texture, &depth_view_desc, &state->render_target_depth);

        D3D11_DEPTH_STENCIL_DESC depth_stencil_settings;
        depth_stencil_settings.DepthEnable = 1;
        depth_stencil_settings.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
        depth_stencil_settings.DepthFunc = D3D11_COMPARISON_LESS;
        depth_stencil_settings.StencilEnable = 0;
        depth_stencil_settings.StencilReadMask;
        depth_stencil_settings.StencilWriteMask;
        depth_stencil_settings.FrontFace = {D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_COMPARISON_ALWAYS};
        depth_stencil_settings.BackFace = {D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_COMPARISON_ALWAYS};

        ID3D11DepthStencilState *depth_state;
        device->CreateDepthStencilState(&depth_stencil_settings, &depth_state);
        context->OMSetDepthStencilState(depth_state, 1);

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

        state->environment = load_wexp(device, memory->read_file, "assets/environment.wexp", state->phong_shader->vertex_file);
        state->player      = load_wexp(device, memory->read_file, "assets/sphere.wexp",      state->phong_shader->vertex_file);
        state->tex_white   = load_texture(device, context, &mempool, memory->read_file, "assets/blockout_white.bmp");
        state->tex_yellow  = load_texture(device, context, &mempool, memory->read_file, "assets/blockout_yellow.bmp");

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
#if WINDY_INTERNAL
    reload_shader(state->phong_shader, memory->reload_if_changed, device, "phong");
#endif
    set_active_shader(context, state->phong_shader);

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

    context->OMSetRenderTargets(1, &state->render_target_rgb, state->render_target_depth);
    r32 ClearColor[] = {0.06f, 0.5f, 0.8f, 1.f};
    context->ClearRenderTargetView(state->render_target_rgb, ClearColor);
    context->ClearDepthStencilView(state->render_target_depth, D3D11_CLEAR_DEPTH, 1.f, 1);


    set_active_mesh(context, &state->environment);
    set_active_texture(context, &state->tex_white);
    //set_active_shader(context, phong_shader);

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
}
