struct VS_OUTPUT
{
    float4 proj_pos : SV_POSITION;
    float2 txc      : TEXCOORD0;
};

#if VERTEX_HLSL // ---------------------------------------------------
//#pragma pack_matrix(row_major)

cbuffer Matrices: register(b0)
{
    float4x4 model;
    float4x4 camera;
    float4x4 projection;
}


VS_OUTPUT
main(uint vert_ID : SV_VertexID)
{
    VS_OUTPUT output;
    float3 ps[] = {
        {-1.f, -1.f, 0.f},     //  2 —— 3
        { 1.f, -1.f, 0.f},     //  | \  |
        {-1.f,  1.f, 0.f},     //  |  \ |
        { 1.f,  1.f, 0.f},     //  0 —— 1
    };

    output.proj_pos = float4(ps[vert_ID], 1.f);
    float4 world_pos = in output.proj_pos;

    output.txc = (ps[vert_ID].xy / 2.f) + 0.5f;
    output.txc.y = 1.f - output.txc.y;
    return(output);
}

#elif PIXEL_HLSL // --------------------------------------------------
#include "hlsl_defines.h"

SamplerState background_sampler_state;
Texture2D    background_texture;

float4
main(VS_OUTPUT input) : SV_TARGET
{
    float4 output = background_texture.Sample(background_sampler_state, input.txc);
    return output;
}

#endif
