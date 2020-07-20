/* ========================================================================
   $File: $
   $Date: $
   $Revision: $
   $Creator: Davide Stasio $
   $Notice: (C) Copyright 2020 by Davide Stasio. All Rights Reserved. $
   ======================================================================== */
#include "win32_renderer_d3d11.h"

inline void
cat(char *src0, char *src1, char *dest)
{
    while (*src0)  *(dest++) = *(src0++);
    while (*src1)  *(dest++) = *(src1++);
    *dest = '\0';
}

PLATFORM_RELOAD_SHADER(d3d11_reload_shader)
{
    D11_Renderer *d11 = (D11_Renderer *)renderer->platform;

    char vertex_path[MAX_PATH] = {};
    char  pixel_path[MAX_PATH] = {};
    cat("assets\\", name, vertex_path);
    cat(vertex_path, ".vsh", vertex_path);
    cat("assets\\", name,  pixel_path);
    cat( pixel_path, ".psh",  pixel_path);
    shader->vertex_file.path = vertex_path;
    shader->pixel_file.path  =  pixel_path;

    if(win32_reload_file_if_changed(&shader->vertex_file))
        d11->device->CreateVertexShader(shader->vertex_file.data, shader->vertex_file.size, 0, (ID3D11VertexShader **)&shader->vertex);

    if(win32_reload_file_if_changed(&shader->pixel_file))
        d11->device->CreatePixelShader(shader->pixel_file.data, shader->pixel_file.size, 0, (ID3D11PixelShader **)&shader->pixel);
}

PLATFORM_LOAD_RENDERER(win32_load_d3d11)
{
    D11_Renderer *d11 = (D11_Renderer *)renderer->platform;

    D3D11_VIEWPORT Viewport = {};
    Viewport.TopLeftX = 0;
    Viewport.TopLeftY = 0;
    Viewport.Width = WIDTH;
    Viewport.Height = HEIGHT;
    Viewport.MinDepth = 0.f;
    Viewport.MaxDepth = 1.f;
    d11->context->RSSetViewports(1, &Viewport);

    //
    // allocating rgb and depth buffers
    //
    D3D11_TEXTURE2D_DESC depth_buffer_desc = {};
    depth_buffer_desc.Width = WIDTH;
    depth_buffer_desc.Height = HEIGHT;
    depth_buffer_desc.MipLevels = 1;
    depth_buffer_desc.ArraySize = 1;
    depth_buffer_desc.Format = DXGI_FORMAT_D32_FLOAT;
    depth_buffer_desc.SampleDesc.Count = 1;
    depth_buffer_desc.SampleDesc.Quality = 0;
    depth_buffer_desc.Usage = D3D11_USAGE_DEFAULT;
    depth_buffer_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    depth_buffer_desc.MiscFlags = 0;

    ID3D11Texture2D *depth_texture;
    d11->device->CreateTexture2D(&depth_buffer_desc, 0, &depth_texture);

    { // Depth states.
        D3D11_DEPTH_STENCIL_VIEW_DESC depth_view_desc = {DXGI_FORMAT_D32_FLOAT, D3D11_DSV_DIMENSION_TEXTURE2D};
        d11->device->CreateRenderTargetView(d11->backbuffer, 0, &d11->render_target_rgb);
        d11->device->CreateDepthStencilView(depth_texture, &depth_view_desc, &d11->render_target_depth);

        D3D11_DEPTH_STENCIL_DESC depth_stencil_settings;
        depth_stencil_settings.DepthEnable    = 1;
        depth_stencil_settings.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
        depth_stencil_settings.DepthFunc      = D3D11_COMPARISON_LESS;
        depth_stencil_settings.StencilEnable  = 0;
        depth_stencil_settings.StencilReadMask;
        depth_stencil_settings.StencilWriteMask;
        depth_stencil_settings.FrontFace      = {D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_COMPARISON_ALWAYS};
        depth_stencil_settings.BackFace       = {D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_COMPARISON_ALWAYS};

        d11->device->CreateDepthStencilState(&depth_stencil_settings, &d11->depth_nostencil_state);

        depth_stencil_settings.DepthEnable    = 0;
        depth_stencil_settings.DepthFunc      = D3D11_COMPARISON_ALWAYS;

        d11->device->CreateDepthStencilState(&depth_stencil_settings, &d11->nodepth_nostencil_state);
    }

    ID3D11SamplerState *sampler;
    D3D11_SAMPLER_DESC sampler_desc = {};
    sampler_desc.Filter = D3D11_FILTER_ANISOTROPIC;//D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_MIRROR;
    sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_MIRROR;
    sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_MIRROR;
    sampler_desc.MipLODBias = -1;
    sampler_desc.MaxAnisotropy = 16;
    sampler_desc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
    sampler_desc.MinLOD = 0;
    sampler_desc.MaxLOD = 100;
    d11->device->CreateSamplerState(&sampler_desc, &sampler);
    d11->context->PSSetSamplers(0, 1, &sampler);

    //
    // constant buffer setup
    //
    D3D11_BUFFER_DESC MatrixBufferDesc = {};
    MatrixBufferDesc.ByteWidth = 3*sizeof(m4);
    MatrixBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    MatrixBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    MatrixBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    MatrixBufferDesc.MiscFlags = 0;
    MatrixBufferDesc.StructureByteStride = sizeof(m4);
    d11->device->CreateBuffer(&MatrixBufferDesc, 0, &d11->matrix_buff);
    d11->context->VSSetConstantBuffers(0, 1, &d11->matrix_buff);

    D3D11_BUFFER_DESC light_buff_desc = {};
    light_buff_desc.ByteWidth = 16*3;
    light_buff_desc.Usage = D3D11_USAGE_DYNAMIC;
    light_buff_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    light_buff_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    light_buff_desc.MiscFlags = 0;
    light_buff_desc.StructureByteStride = sizeof(v3);
    d11->device->CreateBuffer(&light_buff_desc, 0, &d11->light_buff);
    d11->context->PSSetConstantBuffers(0, 1, &d11->light_buff);

    //
    // rasterizer set-up
    //
    D3D11_RASTERIZER_DESC raster_settings = {};
    raster_settings.FillMode = D3D11_FILL_SOLID;
    raster_settings.CullMode = D3D11_CULL_BACK;
    raster_settings.FrontCounterClockwise = 1;
    raster_settings.DepthBias = 0;
    raster_settings.DepthBiasClamp = 0;
    raster_settings.SlopeScaledDepthBias = 0;
    raster_settings.DepthClipEnable = 1;
    raster_settings.ScissorEnable = 0;
    raster_settings.MultisampleEnable = 0;
    raster_settings.AntialiasedLineEnable = 0;

    ID3D11RasterizerState *raster_state = 0;
    d11->device->CreateRasterizerState(&raster_settings, &raster_state);
    d11->context->RSSetState(raster_state);

    //
    // Alpha blending.
    //
    CD3D11_BLEND_DESC blend_state_desc = {};
    blend_state_desc.RenderTarget[0].BlendEnable = 1;
    //blend_state_desc.RenderTarget[0].LogicOpEnable = 0;
    blend_state_desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    blend_state_desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blend_state_desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blend_state_desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    blend_state_desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    blend_state_desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    //blend_state_desc.RenderTarget[0].LogicOp;
    blend_state_desc.RenderTarget[0].RenderTargetWriteMask = 0x0F;

    ID3D11BlendState *blend_state = 0;
    d11->device->CreateBlendState(&blend_state_desc, &blend_state);
    d11->context->OMSetBlendState(blend_state, 0, 0xFFFFFFFF);
}

