/* ========================================================================
   $File: $
   $Date: $
   $Revision: $
   $Creator: Casey Muratori $
   $Notice: (C) Copyright 2014 by Molly Rocket, Inc. All Rights Reserved. $
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
        *State = {};
        memory_pool Pool = {};
        Pool.Base = (u8 *)Memory->Storage;
        Pool.Size = Memory->StorageSize;
        Pool.Used = sizeof(game_state);
        //
        // sending data to gpu
        //
        vertex_shader_input Triangle[] = {
             1.f, -1.f, 0.f,  1.f, 1.f,  // bottom-right
            -1.f, -1.f, 0.f,  0.f, 1.f,  // bottom-left
            -1.f,  1.f, 0.f,  0.f, 0.f,  // top-left

            -1.f,  1.f, 0.f,  0.f, 0.f,  // top-left
             1.f,  1.f, 0.f,  1.f, 0.f,  // top-right
             1.f, -1.f, 0.f,  1.f, 1.f   // bottom-right
        };
        //vertex_shader_input Triangle[] = {
        //     1.1f, 0.f, 0.f,  1.f, 1.f,
        //    0.f, 0.f, 0.f,  0.f, 1.f,
        //    0.f,  1.1f, 0.f,  0.f, 0.f,

        //    0.f,  1.1f, 0.f,  0.f, 0.f,
        //     1.1f,  1.1f, 0.f,  1.f, 0.f,
        //     1.1f, 0.f, 0.f,  1.f, 1.f
        //};
        D3D11_SUBRESOURCE_DATA TriangleData = {(void *)Triangle};
        D3D11_BUFFER_DESC VertexBufferDescription = {};
        VertexBufferDescription.ByteWidth = sizeof(Triangle);
        VertexBufferDescription.Usage = D3D11_USAGE_IMMUTABLE;
        VertexBufferDescription.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        VertexBufferDescription.CPUAccessFlags = 0;
        VertexBufferDescription.MiscFlags = 0;
        VertexBufferDescription.StructureByteStride = sizeof(vertex_shader_input);
        Device->CreateBuffer(&VertexBufferDescription, &TriangleData, &State->VertexBuffer);

        D3D11_BUFFER_DESC ConstantBufferDescription = {};
        ConstantBufferDescription.ByteWidth = 16;
        ConstantBufferDescription.Usage = D3D11_USAGE_DYNAMIC;
        ConstantBufferDescription.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        ConstantBufferDescription.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        ConstantBufferDescription.MiscFlags = 0;
        ConstantBufferDescription.StructureByteStride = 0;
        Device->CreateBuffer(&ConstantBufferDescription, 0, &State->ConstantBuffer);

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
        Context->PSSetConstantBuffers(0, 1, &State->ConstantBuffer);

#define ReadOffset(Pointer, Offset, Type) (*(Type*)(Pointer + Offset))
        bitmap_header *ImageBMP = (bitmap_header *)Memory->ReadFile("assets/sampletexture.bmp").Data;
        image_texture Texture = {};
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

        D3D11_SUBRESOURCE_DATA TextureSubresource = {};
        TextureSubresource.pSysMem = Texture.Bytes;
        TextureSubresource.SysMemPitch = Texture.Width*4;

        ID3D11Texture2D *TextureHandle;
        Device->CreateTexture2D(&TextureDescription, &TextureSubresource, &TextureHandle);
        ID3D11ShaderResourceView *TextureResource;
        Device->CreateShaderResourceView(TextureHandle, 0, &TextureResource);
        Context->PSSetShaderResources(0, 1, &TextureResource);

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

        Memory->IsInitialized = true;
    }

    D3D11_MAPPED_SUBRESOURCE MappedBuffer = {};
    Context->Map(State->ConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedBuffer);
    *(u32 *)MappedBuffer.pData = Input->Held.Up;
    Context->Unmap(State->ConstantBuffer, 0);

    r32 ClearColor[] = {0.06f, 0.05f, 0.08f, 1.f};
    Context->OMSetRenderTargets(1, &View, 0);
    Context->ClearRenderTargetView(View, ClearColor);
    Context->Draw(6, 0);
}
