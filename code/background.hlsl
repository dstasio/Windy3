struct VS_OUTPUT
{
    float4 proj_pos : SV_POSITION;
    //float2 txc      : TEXCOORD0;
    float4 color : COLOR;
};

#if VERTEX_HLSL // ---------------------------------------------------
//#pragma pack_matrix(row_major)

cbuffer Matrices: register(b0)
{
    float4x4 camera;
    float4x4 projection;
}

VS_OUTPUT
main(uint vert_ID : SV_VertexID)
{
    VS_OUTPUT output;
    output.color = float4(0.f, 0.f, 0.f, 1.f);
    float3 ps[] = {
        {-1.f, -1.f, 0.f},
        { 1.f, -1.f, 0.f},
        {-1.f,  1.f, 0.f},
        { 1.f,  1.f, 0.f},
    };
    float4 cols[] = {
        {1.f, 1.f, 1.f, 1.f},
        {1.f, 0.f, 0.f, 1.f},
        {0.f, 1.f, 0.f, 1.f},
        {0.f, 0.f, 1.f, 1.f},
    };

    output.proj_pos = float4(ps[vert_ID], 1.f);
    output.color = cols[vert_ID];
    return(output);
}

#elif PIXEL_HLSL // --------------------------------------------------
#include "hlsl_defines.h"

SamplerState background_sampler_state;
Texture2D    background_texture;

struct PS_OUTPUT
{
    float4 color : SV_TARGET;
};

PS_OUTPUT
main(VS_OUTPUT input)
{
    PS_OUTPUT output; 
    output.color = input.color;
        //output.color *= SampleTexture.Sample(TextureSamplerState, input.txc);
    return(output);
}

#endif
