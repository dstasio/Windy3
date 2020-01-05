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
};

GAME_UPDATE_AND_RENDER(WindyUpdateAndRender)
{
    game_state *State = (game_state *)Memory->Storage;
    if(!Memory->IsInitialized)
    {
        *State = {};
        //
        // sending data to gpu
        //
        vertex_shader_input Triangle[] = {
            -0.5f, -0.5f, 0.f,
             0.f,   0.5f, 0.f,
             0.5f, -0.5f, 0.f
        };
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
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
                0, D3D11_INPUT_PER_VERTEX_DATA, 0}
        };
        Device->CreateInputLayout(InputDescription, 1,
                                  VertexBytes.Data, VertexBytes.Size,
                                  &InputLayout);

        // TODO(dave): can strides and offsets be 0???
        u32 Stride = sizeof(vertex_shader_input);
        u32 Offset = 0;
        Context->IASetVertexBuffers(0, 1, &State->VertexBuffer, &Stride, &Offset);
        Context->IASetInputLayout(InputLayout);
        Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        Context->PSSetConstantBuffers(0, 1, &State->ConstantBuffer);

        Memory->IsInitialized = true;
    }

    D3D11_MAPPED_SUBRESOURCE MappedBuffer = {};
    Context->Map(State->ConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedBuffer);
    *(u32 *)MappedBuffer.pData = Input->Held.Up;
    Context->Unmap(State->ConstantBuffer, 0);

    r32 ClearColor[] = {0.06f, 0.05f, 0.08f, 1.f};
    Context->OMSetRenderTargets(1, &View, 0);
    Context->ClearRenderTargetView(View, ClearColor);
    Context->Draw(3, 0);
}
