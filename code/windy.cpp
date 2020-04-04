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

struct vertex_shader_input
{
    r32 x, y, z;
    r32 tx, ty;
};

GAME_UPDATE_AND_RENDER(WindyUpdateAndRender)
{
    game_state *State = (game_state *)Memory->Storage;
    if(!Memory->IsInitialized)
    {
        memory_pool Pool = {};
        Pool.Base = (u8 *)Memory->Storage;
        Pool.Size = Memory->StorageSize;
        Pool.Used = sizeof(game_state);

        D3D11_VIEWPORT Viewport = {};
        Viewport.TopLeftX = 0;
        Viewport.TopLeftY = 0;
        Viewport.Width = WIDTH;
        Viewport.Height = HEIGHT;
        Viewport.MinDepth = 0.f;
        Viewport.MaxDepth = 1.f;
        Context->RSSetViewports(1, &Viewport);

        //
        // allocating rgb and depth buffers
        //
        D3D11_TEXTURE2D_DESC depth_buffer_desc = {};
        depth_buffer_desc.Width = WIDTH;
        depth_buffer_desc.Height = HEIGHT;
        depth_buffer_desc.MipLevels = 1;
        depth_buffer_desc.ArraySize = 1;
        depth_buffer_desc.Format = DXGI_FORMAT_D16_UNORM;
        depth_buffer_desc.SampleDesc.Count = 1;
        depth_buffer_desc.SampleDesc.Quality = 0;
        depth_buffer_desc.Usage = D3D11_USAGE_DEFAULT;
        depth_buffer_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
        depth_buffer_desc.MiscFlags = 0;

        ID3D11Texture2D *depth_texture;
        Device->CreateTexture2D(&depth_buffer_desc, 0, &depth_texture);

        D3D11_DEPTH_STENCIL_VIEW_DESC depth_view_desc = {DXGI_FORMAT_D16_UNORM, D3D11_DSV_DIMENSION_TEXTURE2D};
        Device->CreateRenderTargetView(rendering_backbuffer, 0, &State->render_target_rgb);
        Device->CreateDepthStencilView(depth_texture, &depth_view_desc, &State->render_target_depth);

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
        Device->CreateDepthStencilState(&depth_stencil_settings, &depth_state);
        Context->OMSetDepthStencilState(depth_state, 1);


        //vertex_shader_input cube[] = {
        //    -1, -1,  1,  0.f, 0.f,     // positive z
        //     1, -1,  1,  0.f, 1.f,
        //     1,  1,  1,  1.f, 1.f,
        //    // 1,  1,  1,  1.f, 1.f,
        //    -1,  1,  1,  1.f, 0.f,
        //    //-1, -1,  1,  0.f, 0.f,

        //    //-1, -1,  1,  0.f, 0.f,      // negative x
        //    //-1,  1,  1,  0.f, 1.f,
        //    -1,  1, -1,  1.f, 1.f,
        //    //-1,  1, -1,  1.f, 1.f,
        //    -1, -1, -1,  1.f, 0.f,
        //    //-1, -1,  1,  0.f, 0.f,

        //    //-1, -1,  1,  0.f, 0.f,      // negative y
        //    //-1, -1, -1,  0.f, 1.f,
        //     1, -1, -1,  1.f, 1.f,
        //    // 1, -1, -1,  1.f, 1.f,
        //    // 1, -1,  1,  1.f, 0.f,
        //    //-1, -1,  1,  0.f, 0.f,

        //    //-1, -1, -1,  0.f, 0.f,      // negative z
        //    //-1,  1, -1,  0.f, 1.f,
        //     1,  1, -1,  1.f, 1.f,
        //    // 1,  1, -1,  1.f, 1.f,
        //    // 1, -1, -1,  1.f, 0.f,
        //    //-1, -1, -1,  0.f, 0.f,

        //    //-1,  1, -1,  0.f, 0.f,      // positive y
        //    //-1,  1,  1,  0.f, 1.f,
        //    // 1,  1,  1,  1.f, 1.f,
        //    // 1,  1,  1,  1.f, 1.f, 
        //    // 1,  1, -1,  1.f, 0.f,
        //    //-1,  1, -1,  0.f, 0.f,

        //    // 1, -1,  1,  0.f, 0.f,      //positive x
        //    // 1, -1, -1,  0.f, 1.f,
        //    // 1,  1, -1,  1.f, 1.f,
        //    // 1,  1, -1,  1.f, 1.f,
        //    // 1,  1,  1,  1.f, 0.f,
        //    // 1, -1,  1,  0.f, 0.f
        //};

        //u16 indices[] = {
        //    0, 1, 2,  2, 3, 0,
        //    0, 3, 4,  4, 5, 0,
        //    0, 5, 6,  6, 1, 0,
        //    5, 4, 7,  7, 6, 5,
        //    4, 3, 2,  2, 7, 4,
        //    1, 6, 7,  7, 2, 1
        //};

        Wexp_header *cube = (Wexp_header *)Memory->ReadFile("assets/cube.wexp").Data;
        Assert(cube->signature == 0x7877);
        u32 vertices_size = cube->indices_offset - cube->vert_offset;
        u32 indices_size  = cube->eof_offset - cube->indices_offset;

        D3D11_SUBRESOURCE_DATA TriangleData = {(void *)((u8 *)cube + cube->vert_offset)};
        D3D11_BUFFER_DESC vertex_buff_desc = {};
        vertex_buff_desc.ByteWidth = vertices_size;
        vertex_buff_desc.Usage = D3D11_USAGE_IMMUTABLE;
        vertex_buff_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        vertex_buff_desc.CPUAccessFlags = 0;
        vertex_buff_desc.MiscFlags = 0;
        vertex_buff_desc.StructureByteStride = 8*sizeof(r32);
        Device->CreateBuffer(&vertex_buff_desc, &TriangleData, &State->vertex_buff);

        //
        // input layout description
        //
        ID3D11InputLayout *InputLayout;
        D3D11_INPUT_ELEMENT_DESC InputDescription[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0}
        };
        Device->CreateInputLayout(InputDescription, 3,
                                  VertexBytes.Data, VertexBytes.Size,
                                  &InputLayout);

        //
        // sending indices to gpu
        //
        D3D11_SUBRESOURCE_DATA index_data = {(void *)((u8 *)cube + cube->indices_offset)};
        D3D11_BUFFER_DESC index_buff_desc = {};
        index_buff_desc.ByteWidth = indices_size;
        index_buff_desc.Usage = D3D11_USAGE_IMMUTABLE;
        index_buff_desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
        Device->CreateBuffer(&index_buff_desc, &index_data, &State->index_buff);

        // can strides and offsets be 0???
        // edit: apparently not!
        u32 Offset = 0;
        Context->IASetVertexBuffers(0, 1, &State->vertex_buff, &vertex_buff_desc.StructureByteStride, &Offset);
        Context->IASetInputLayout(InputLayout);
        Context->IASetIndexBuffer(State->index_buff, DXGI_FORMAT_R16_UINT, 0);
        Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        //
        // bitmap texture loading
        //
        Bitmap_header *ImageBMP = (Bitmap_header *)Memory->ReadFile("assets/sampletexture.bmp").Data;
        image_data Texture = {};
        Texture.Bytes = (u8 *)ImageBMP + ImageBMP->DataOffset;
        Texture.Width = ImageBMP->Width;
        Texture.Height = ImageBMP->Height;

        Assert(ImageBMP->BitsPerPixel == 24);
        Assert(ImageBMP->Compression == 0);

        u32 ImageSizeInBytes = Texture.Width*Texture.Height*4;
        u8 *ImageOrderedBytes = PushArray(&Pool, ImageSizeInBytes, u8);
        for(u32 Y = 0;
            Y < Texture.Height;
            ++Y)
        {
            for(u32 X = 0;
                X < Texture.Width;
                ++X)
            {
                u32 BytesPerScanline = Texture.Width*3;
                u32 PaddingPerScanline = (4 - BytesPerScanline % 4) % 4;
                u32 SourceOffset = ((Texture.Height - Y - 1)*
                                    (BytesPerScanline + PaddingPerScanline) +
                                    X*3);
                u32 DestOffset = Y*Texture.Width*4 + X*4;
                u32 *SourcePixel = (u32 *)((u8 *)Texture.Bytes + SourceOffset);
                u32 *DestPixel = (u32 *)(ImageOrderedBytes + DestOffset);
                u32 RMask = 0x000000FF;
                u32 GMask = 0x0000FF00;
                u32 BMask = 0x00FF0000;
                *DestPixel = ((RMask & (*SourcePixel >> 16)) |
                              (GMask & (*SourcePixel)) |
                              (BMask & (*SourcePixel >> 8)) |
                              0xFF000000);
            }
        }
        Texture.Bytes = ImageOrderedBytes;

        //
        // texture to gpu
        //
        D3D11_TEXTURE2D_DESC TextureDescription = {};
        TextureDescription.Width = Texture.Width;
        TextureDescription.Height = Texture.Height;
        TextureDescription.MipLevels = 1;
        TextureDescription.ArraySize = 1;
        TextureDescription.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        TextureDescription.SampleDesc.Count = 1;
        TextureDescription.SampleDesc.Quality = 0;
        TextureDescription.Usage = D3D11_USAGE_IMMUTABLE;
        TextureDescription.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        TextureDescription.MiscFlags = 0;

        texture Tex = {};

        D3D11_SUBRESOURCE_DATA TextureSubresource = {};
        TextureSubresource.pSysMem = Texture.Bytes;
        TextureSubresource.SysMemPitch = Texture.Width*4;

        Device->CreateTexture2D(&TextureDescription, &TextureSubresource, &Tex.Handle);
        Device->CreateShaderResourceView(Tex.Handle, 0, &Tex.Resource);
        Context->PSSetShaderResources(0, 1, &Tex.Resource);

        ID3D11SamplerState *Sampler;
        D3D11_SAMPLER_DESC SamplerDescription = {};
        SamplerDescription.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        SamplerDescription.AddressU = D3D11_TEXTURE_ADDRESS_MIRROR;
        SamplerDescription.AddressV = D3D11_TEXTURE_ADDRESS_MIRROR;
        SamplerDescription.AddressW = D3D11_TEXTURE_ADDRESS_MIRROR;
        SamplerDescription.ComparisonFunc = D3D11_COMPARISON_NEVER;
        SamplerDescription.MinLOD = 0;
        SamplerDescription.MaxLOD = 1000;
        Device->CreateSamplerState(&SamplerDescription, &Sampler);
        Context->PSSetSamplers(0, 1, &Sampler);

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
        Device->CreateBuffer(&MatrixBufferDesc, 0, &State->matrix_buff);
        Context->VSSetConstantBuffers(0, 1, &State->matrix_buff);

        D3D11_RASTERIZER_DESC raster_settings = {};
        raster_settings.FillMode = D3D11_FILL_SOLID;
        raster_settings.CullMode = D3D11_CULL_NONE;
        raster_settings.FrontCounterClockwise = 1;
        raster_settings.DepthBias = 0;
        raster_settings.DepthBiasClamp = 0;
        raster_settings.SlopeScaledDepthBias = 0;
        raster_settings.DepthClipEnable = 1;
        raster_settings.ScissorEnable = 0;
        raster_settings.MultisampleEnable = 0;
        raster_settings.AntialiasedLineEnable = 0;

        ID3D11RasterizerState *raster_state = 0;
        Device->CreateRasterizerState(&raster_settings, &raster_state);
        Context->RSSetState(raster_state);

        //
        // camera set-up
        //
        State->main_cam.pos    = {0.f, -3.f, 2.f};
        State->main_cam.target = {0.f,  0.f, 0.f};
        State->main_cam.up     = {0.f,  0.f, 1.f};
        State->cam_radius = 5.f;

        Memory->IsInitialized = true;
    }

    r32 speed = Input->Held.space ? 10.f : 3.f;
    if (Input->Held.up)    State->theta += (PI/4.f)*dtime;
    if (Input->Held.down)  State->theta -= (PI/4.f)*dtime;
    if (Input->Held.w)     State->cam_radius -= speed*dtime;
    if (Input->Held.s)     State->cam_radius += speed*dtime;
    if (Input->Held.a)     State->cam_htheta -= speed*(PI/12.f)*dtime;
    if (Input->Held.d)     State->cam_htheta += speed*(PI/12.f)*dtime;
    if (Input->Held.shift) State->cam_vtheta += speed*(PI/12.f)*dtime;
    if (Input->Held.ctrl)  State->cam_vtheta -= speed*(PI/12.f)*dtime;

    State->main_cam.pos.z = Sin(State->cam_vtheta);
    State->main_cam.pos.x = Cos(State->cam_htheta) * Cos(State->cam_vtheta);
    State->main_cam.pos.y = Sin(State->cam_htheta) * Cos(State->cam_vtheta);
    State->main_cam.pos = Normalize(State->main_cam.pos)*State->cam_radius;

    D3D11_MAPPED_SUBRESOURCE MatrixMap = {};
    Context->Map(State->matrix_buff, 0, D3D11_MAP_WRITE_DISCARD, 0, &MatrixMap);
    m4 *matrix_buffer = (m4 *)MatrixMap.pData;
    matrix_buffer[0] = Pitch_m4(State->theta);
    matrix_buffer[1] = Camera_m4(State->main_cam.pos, State->main_cam.target, State->main_cam.up);
    matrix_buffer[2] = Perspective_m4(DegToRad*60.f, (r32)WIDTH/(r32)HEIGHT, 0.01f, 100.f);
    Context->Unmap(State->matrix_buff, 0);

    Context->OMSetRenderTargets(1, &State->render_target_rgb, State->render_target_depth);
    r32 ClearColor[] = {0.06f, 0.05f, 0.08f, 1.f};
    Context->ClearRenderTargetView(State->render_target_rgb, ClearColor);
    Context->ClearDepthStencilView(State->render_target_depth, D3D11_CLEAR_DEPTH, 1.f, 1);
    // @todo: keep index count for each mesh
    Context->DrawIndexed(36, 0, 0);
}
