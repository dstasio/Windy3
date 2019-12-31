/* ========================================================================
   $File: $
   $Date: $
   $Revision: $
   $Creator: Casey Muratori $
   $Notice: (C) Copyright 2014 by Molly Rocket, Inc. All Rights Reserved. $
   ======================================================================== */
#include <d3d11.h>
#include "windy_platform.h"

struct vertex_shader_input
{
    r32 x, y, z;
};

GAME_UPDATE_AND_RENDER(WindyUpdateAndRender)
{
    if(!Memory->IsInitialized)
    {
        //
        // sending data to gpu
        //
        ID3D11Buffer *Buffer;
        vertex_shader_input TriangleData[] = {
            -0.5f, -0.5f, 0.f,
             0.f,   0.5f, 0.f,
             0.5f, -0.5f, 0.f
        };
        D3D11_SUBRESOURCE_DATA Data = {(void *)TriangleData};
        D3D11_BUFFER_DESC BufferDescription = {};
        BufferDescription.ByteWidth = sizeof(TriangleData);
        BufferDescription.Usage = D3D11_USAGE_IMMUTABLE;
        BufferDescription.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        BufferDescription.CPUAccessFlags = 0;
        BufferDescription.MiscFlags = 0;
        BufferDescription.StructureByteStride = sizeof(vertex_shader_input);
        Device->CreateBuffer(&BufferDescription, &Data, &Buffer);

        //
        // input layout description
        //
        ID3D11InputLayout *InputLayout;
        D3D11_INPUT_ELEMENT_DESC InputDescription[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
                0, D3D11_INPUT_PER_VERTEX_DATA, 0}
        };
        Device->CreateInputLayout(InputDescription, 1,
                                  VertexBytes.Data, VertexBytes.Size,
                                  &InputLayout);

        // TODO(dave): can strides and offsets be 0???
        u32 Stride = sizeof(vertex_shader_input);
        u32 Offset = 0;
        Context->IASetVertexBuffers(0, 1, &Buffer, &Stride, &Offset);
        Context->IASetInputLayout(InputLayout);
        Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        //Context->OMSetRenderTargets(1, &View, 0);

        Memory->IsInitialized = true;
    }

    r32 ClearColor[] = {0.06f, 0.05f, 0.08f, 1.f};
    Context->OMSetRenderTargets(1, &View, 0);
    Context->ClearRenderTargetView(View, ClearColor);
    Context->Draw(3, 0);
}
