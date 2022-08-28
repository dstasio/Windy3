struct VS_OUTPUT
{
    float4 proj_pos  : SV_POSITION;
    float2 txc       : TEXCOORD0;
    float3 normal    : NORMAL;
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
    output.txc = input.txc;
    //output.txc = input.pos.xy;
    output.normal = input.normal;

    return(output);
}

#elif PIXEL_HLSL // --------------------------------------------------
#include "hlsl_defines.h"

SamplerState TextureSamplerState;

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

Texture2D SampleTexture;


float3
light_intensity(float3 color, float3 dir, float3 eyedir, float3 normal)
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
    float3 normal = normalize(input.normal);
    float3 eyedir = normalize(eye - input.world_pos);

    float3 final_lighting = float3(0.f, 0.f, 0.f);
    if (!(flags & PHONG_FLAG_UNLIT))
    {
        for (uint light_index = 0; light_index < light_count; ++light_index) {
            float3 light_dir;
            if (light_type[light_index] == PHONG_LIGHT_POINT)
                light_dir = normalize(lightpos[light_index] - input.world_pos);
            else // light_type[light_index] == PHONG_LIGHT_DIRECTIONAL
                light_dir = lightpos[light_index];

            final_lighting += light_intensity(light_color[light_index], light_dir, eyedir, normal);
        }
    }
    else {
        final_lighting = float3(1.f, 1.f, 1.f);
    }

    if (flags & PHONG_FLAG_SOLIDCOLOR)
        output.color = float4(final_lighting, 1.f) * float4(solid_color, 1.f);
    else
        output.color = float4(final_lighting, 1.f) * SampleTexture.Sample(TextureSamplerState, input.txc);

    return(output);
}

#endif
