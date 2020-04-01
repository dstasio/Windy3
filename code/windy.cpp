/* ========================================================================
   $File: $
   $Date: $
   $Revision: $
   $Creator: Davide Stasio $
   $Notice: (C) Copyright 2014 by Davide Stasio. All Rights Reserved. $
   ======================================================================== */
#include <d3d11.h>
#include "windy.h"
#include <string.h>

struct vertex_shader_input
{
    r32 x, y, z;
    r32 tx, ty;
};

#pragma pack(push, 1)
struct bitmap_header
{
    u16 Signature;
    u32 FileSize;
    u32 Reserved;
    u32 DataOffset;
    u32 InfoHeaderSize;
    u32 Width;
    u32 Height;
    u16 Planes;
    u16 BitsPerPixel;
    u32 Compression;
    u32 ImageSize;
    u32 XPixelsPerMeter;
    u32 YPixelsPerMeter;
    u32 ColorsUsed;
    u32 ImportantColors;
};
#pragma pack(pop)

GAME_UPDATE_AND_RENDER(WindyUpdateAndRender)
{
    game_state *State = (game_state *)Memory->Storage;
    if(!Memory->IsInitialized)
    {
        memory_pool Pool = {};
        Pool.Base = (u8 *)Memory->Storage;
        Pool.Size = Memory->StorageSize;
        Pool.Used = sizeof(game_state);

        //
        // allocating depth buffer
        //
        D3D11_TEXTURE2D_DESC depth_buffer_desc = {};
        depth_buffer_desc.Width = WIDTH;
        depth_buffer_desc.Height = HEIGHT;
        depth_buffer_desc.MipLevels = 1;
        depth_buffer_desc.ArraySize = 1;
        depth_buffer_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        depth_buffer_desc.SampleDesc.Count = 1;
        depth_buffer_desc.SampleDesc.Quality = 0;
        depth_buffer_desc.Usage = D3D11_USAGE_IMMUTABLE;
        depth_buffer_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        depth_buffer_desc.MiscFlags = 0;

        //Device->CreateTexture2D(&TextureDescription, &TextureSubresource, &Tex.Handle);

        Device->CreateRenderTargetView(rendering_backbuffer, 0, &State->render_target_rgb);

        D3D11_VIEWPORT Viewport = {};
        Viewport.TopLeftX = 0;
        Viewport.TopLeftY = 0;
        Viewport.Width = WIDTH;
        Viewport.Height = HEIGHT;
        Context->RSSetViewports(1, &Viewport);

        //
        // sending data to gpu
        //
        //vertex_shader_input Triangle[] = {
        //     1.f, -1.f, 0.f,  1.f, 1.f,  // bottom-right
        //    -1.f, -1.f, 0.f,  0.f, 1.f,  // bottom-left
        //    -1.f,  1.f, 0.f,  0.f, 0.f,  // top-left

        //    -1.f,  1.f, 0.f,  0.f, 0.f,  // top-left
        //     1.f,  1.f, 0.f,  1.f, 0.f,  // top-right
        //     1.f, -1.f, 0.f,  1.f, 1.f   // bottom-right
        //};
        vertex_shader_input cube[] = {
            -1.f, -1.f,  1.f,  0.f, 0.f,      // positive z
             1.f, -1.f,  1.f,  0.f, 1.f,
             1.f,  1.f,  1.f,  1.f, 1.f,
             1.f,  1.f,  1.f,  1.f, 1.f,
            -1.f,  1.f,  1.f,  1.f, 0.f,
            -1.f, -1.f,  1.f,  0.f, 0.f,

            -1.f, -1.f,  1.f,  0.f, 0.f,      // negative x
            -1.f,  1.f,  1.f,  0.f, 1.f,
            -1.f,  1.f, -1.f,  1.f, 1.f,
            -1.f,  1.f, -1.f,  1.f, 1.f,
            -1.f, -1.f, -1.f,  1.f, 0.f,
            -1.f, -1.f,  1.f,  0.f, 0.f,

            -1.f, -1.f,  1.f,  0.f, 0.f,      // negative y
            -1.f, -1.f, -1.f,  0.f, 1.f,
             1.f, -1.f, -1.f,  1.f, 1.f,
             1.f, -1.f, -1.f,  1.f, 1.f,
             1.f, -1.f,  1.f,  1.f, 0.f,
            -1.f, -1.f,  1.f,  0.f, 0.f,

            -1.f, -1.f, -1.f,  0.f, 0.f,      // negative z
            -1.f,  1.f, -1.f,  0.f, 1.f,
             1.f,  1.f, -1.f,  1.f, 1.f,
             1.f,  1.f, -1.f,  1.f, 1.f,
             1.f, -1.f, -1.f,  1.f, 0.f,
            -1.f, -1.f, -1.f,  0.f, 0.f,

            -1.f,  1.f, -1.f,  0.f, 0.f,      // positive y
            -1.f,  1.f,  1.f,  0.f, 1.f,
             1.f,  1.f,  1.f,  1.f, 1.f,
             1.f,  1.f,  1.f,  1.f, 1.f, 
             1.f,  1.f, -1.f,  1.f, 0.f,
            -1.f,  1.f, -1.f,  0.f, 0.f,

             1.f, -1.f,  1.f,  0.f, 0.f,      //positive x
             1.f,  1.f, -1.f,  0.f, 1.f,
             1.f,  1.f,  1.f,  1.f, 1.f,
             1.f,  1.f,  1.f,  1.f, 1.f,
             1.f, -1.f,  1.f,  1.f, 0.f,
             1.f, -1.f, -1.f,  0.f, 0.f
        };

        D3D11_SUBRESOURCE_DATA TriangleData = {(void *)cube};
        D3D11_BUFFER_DESC VertexBufferDescription = {};
        VertexBufferDescription.ByteWidth = sizeof(cube);
        VertexBufferDescription.Usage = D3D11_USAGE_IMMUTABLE;
        VertexBufferDescription.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        VertexBufferDescription.CPUAccessFlags = 0;
        VertexBufferDescription.MiscFlags = 0;
        VertexBufferDescription.StructureByteStride = sizeof(vertex_shader_input);
        Device->CreateBuffer(&VertexBufferDescription, &TriangleData, &State->VertexBuffer);

        //
        // input layout description
        //
        ID3D11InputLayout *InputLayout;
        D3D11_INPUT_ELEMENT_DESC InputDescription[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0}
        };
        Device->CreateInputLayout(InputDescription, 2,
                                  VertexBytes.Data, VertexBytes.Size,
                                  &InputLayout);

        // TODO(dave): can strides and offsets be 0???
        u32 Stride = sizeof(vertex_shader_input);
        u32 Offset = 0;
        Context->IASetVertexBuffers(0, 1, &State->VertexBuffer, &Stride, &Offset);
        Context->IASetInputLayout(InputLayout);
        Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        //
        // bitmap texture loading
        //
        bitmap_header *ImageBMP = (bitmap_header *)Memory->ReadFile("assets/sampletexture.bmp").Data;
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
        Device->CreateBuffer(&MatrixBufferDesc, 0, &State->MatrixBuffer);
        Context->VSSetConstantBuffers(0, 1, &State->MatrixBuffer);

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

        Memory->IsInitialized = true;
    }

    if (Input->Held.up)    State->theta += (PI/4.f)*dtime;
    if (Input->Held.down)  State->theta -= (PI/4.f)*dtime;
    if (Input->Held.w)     State->main_cam.pos.y += 1*dtime;
    if (Input->Held.s)     State->main_cam.pos.y -= 1*dtime;
    if (Input->Held.a)     State->main_cam.pos.x -= 1*dtime;
    if (Input->Held.d)     State->main_cam.pos.x += 1*dtime;
    if (Input->Held.shift) State->main_cam.pos.z += 1*dtime;
    if (Input->Held.ctrl)  State->main_cam.pos.z -= 1*dtime;

    D3D11_MAPPED_SUBRESOURCE MatrixMap = {};
    Context->Map(State->MatrixBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MatrixMap);
    m4 *matrix_buffer = (m4 *)MatrixMap.pData;
    matrix_buffer[0] = Pitch_m4(State->theta);
    matrix_buffer[1] = Camera_m4(State->main_cam.pos, State->main_cam.target, State->main_cam.up);
    matrix_buffer[2] = Perspective_m4(DegToRad*90.f, WIDTH/HEIGHT, 0.01f, 100.f);
    Context->Unmap(State->MatrixBuffer, 0);

    r32 ClearColor[] = {0.06f, 0.05f, 0.08f, 1.f};
    Context->OMSetRenderTargets(1, &State->render_target_rgb, 0);
    Context->ClearRenderTargetView(State->render_target_rgb, ClearColor);
    Context->Draw(36, 0);
}
