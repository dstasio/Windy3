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
    float4x4 camera_space = mul(projection, mul(camera, model));
    output.proj_pos = mul(camera_space, float4(input.pos, 1.f));
    //output.txc = input.txc;
    output.txc = input.pos.xy;
    output.normal = input.normal;
    output.world_pos = input.pos;

    return(output);
}

#elif PIXEL_HLSL // --------------------------------------------------

SamplerState TextureSamplerState;

struct PS_OUTPUT
{
    float4 color : SV_TARGET;
};

cbuffer Lights: register(b0)
{
    float3 color;
    float3 lightpos;
    float3 eye;
}

Texture2D SampleTexture;

float4
light(float3 color, float3 dir, float3 eyedir, float3 normal)
{
    float ambient = 0.1f;
    float diffuse = max(dot(dir, normal), 0.f);
    float specular = pow(max(dot(reflect(-dir, normal), eyedir), 0.0f), 128);
    return float4(color*(ambient+diffuse+specular*0.3), 1.f);
}

PS_OUTPUT
main(VS_OUTPUT input)
{
    PS_OUTPUT output; 
    float3 normal = normalize(input.normal);
    float3 eyedir = normalize(eye - input.world_pos);
    float3 lightdir = normalize(lightpos - input.world_pos);
    output.color = light(color, lightdir, eyedir, normal) * SampleTexture.Sample(TextureSamplerState, input.txc);
    return(output);
}

#endif
