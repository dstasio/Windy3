struct VS_OUTPUT
{
    float4 proj_pos  : SV_POSITION;
    float3 world_pos : POSITION;
};

#if VERTEX_HLSL // ---------------------------------------------------
//#pragma pack_matrix(row_major)

struct VS_INPUT
{
    float3 pos    : POSITION;
    float3 normal : NORMAL;
    float2 txc    : TEXCOORD;
};

cbuffer Matrices: register(b0)
{
    float4x4 model;
    float4x4 camera;
    float4x4 projection;
}

VS_OUTPUT
main(VS_INPUT input)
{
    VS_OUTPUT output;
    output.world_pos = (float3)mul(model, float4(input.pos, 1.f));
    float4x4 screen_space = mul(projection, camera);
    output.proj_pos = mul(screen_space, float4(output.world_pos, 1.f));

    return(output);
}

#elif PIXEL_HLSL // --------------------------------------------------
#include "hlsl_defines.h"

SamplerState TextureSamplerState;

struct PS_OUTPUT
{
    float dist : SV_TARGET;
};

#if 0
cbuffer Lights: register(b0)
{
    uint   light_count;
    float3 eye;

    uint   light_type [PHONG_MAX_LIGHTS];
    float3 light_color[PHONG_MAX_LIGHTS];
    float3 lightpos   [PHONG_MAX_LIGHTS];
}

Texture2D SampleTexture;
#endif


PS_OUTPUT
main(VS_OUTPUT input)
{
    PS_OUTPUT output = {input.proj_pos.z};

    return(output);
}

#endif
