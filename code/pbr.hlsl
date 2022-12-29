struct VS_OUTPUT
{
    float4 proj_pos         : SV_POSITION;
    float2 txc              : TEXCOORD0;
    float3 normal           : NORMAL;
    float3 world_pos        : POSITION0;
    float3 shadow_space_pos : POSITION1;
    float3 testpos : POSITION2;
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
    float4x4 shadow_space;
}

VS_OUTPUT
main(VS_INPUT input)
{
    float4x4 screen_space   = mul(projection, camera);

    VS_OUTPUT output;
    output.world_pos        = (float3)mul(model, float4(input.pos, 1.f));
    output.proj_pos         = mul(screen_space, float4(output.world_pos, 1.f));
    output.testpos         = mul(screen_space, float4(output.world_pos, 1.f)).xyz;
    output.txc              = input.txc;
    //output.txc            = input.pos.xy;
    output.normal           = input.normal;
    output.shadow_space_pos = mul(shadow_space, float4(output.world_pos, 1.f)).xyz;

    return(output);
}

#elif PIXEL_HLSL // --------------------------------------------------
#include "hlsl_defines.h"

SamplerState texture_sampler_state;
SamplerState  shadow_sampler_state;

struct PS_OUTPUT
{
    float4 color : SV_TARGET;
};

cbuffer Lights: register(b0)
{
    uint   light_count;
    float3 eye;

    uint   light_type [PHONG_MAX_LIGHTS];
    float3 light_color[PHONG_MAX_LIGHTS];
    float3 lightpos   [PHONG_MAX_LIGHTS];
}

cbuffer Settings: register(b1)
{
    unsigned int flags;
    float3 solid_color;         // used if FLAG_SOLIDCOLOR is set
}

Texture2D<float4>  model_texture: register(t0);
Texture2D<float>  shadow_texture: register(t1);

float3
phong(float3 color, float3 dir, float3 eyedir, float3 normal)
{
    float diffuse_coeff = dot(dir, normal);
    float diffuse_mask = sign(diffuse_coeff);
    float diffuse_light = diffuse_coeff * (diffuse_mask * 0.5f + 0.5f);
    float diffuse_shadow = pow((-1.f - diffuse_coeff) * (diffuse_mask * 0.5f - 0.5f), 2.f);

    float ambient = 0.1f + 0.1f * diffuse_shadow;
    float diffuse = diffuse_light;
    float specular = pow(max(dot(reflect(-dir, normal), eyedir), 0.0f), 512);
    return color*(ambient+diffuse+specular*0.2f);
}

PS_OUTPUT
main(VS_OUTPUT input)
{
    PS_OUTPUT output;

    float3 normal        = normalize(input.normal);
    float3 eyedir        = normalize(eye - input.world_pos);
    float3 inv_light_dir = -lightpos[0];
    float3 phong_light   = phong(light_color[0], inv_light_dir, eyedir, normal);

    output.color = float4(phong_light, 1.f);
    return output;
}

#endif
