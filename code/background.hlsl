struct VS_OUTPUT
{
    float4 proj_pos : SV_POSITION;
    float4 look_dir : COLOR;
    float2 txc      : TEXCOORD0;
};

#if VERTEX_HLSL // ---------------------------------------------------
//#pragma pack_matrix(row_major)

struct VS_INPUT
{
    float3 world_dir : POSITION;
    uint   vertex_ID : SV_VertexID;
};

VS_OUTPUT
main(VS_INPUT input)
{
    VS_OUTPUT output;
    float3 ps[] = {
        {-1.f, -1.f, 0.f},     //  2 —— 3
        { 1.f, -1.f, 0.f},     //  | \  |
        {-1.f,  1.f, 0.f},     //  |  \ |
        { 1.f,  1.f, 0.f},     //  0 —— 1
    };

    output.proj_pos = float4(ps[input.vertex_ID], 1.f);

    output.look_dir = float4(input.world_dir, 1.0);

    output.txc = (ps[input.vertex_ID].xy * 0.5f) + 0.5f;
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
    //float4 color = background_texture.Sample(background_sampler_state, input.txc);
    float4 color = input.look_dir;
    return color;
}

#endif
