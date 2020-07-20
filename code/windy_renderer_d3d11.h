#if !defined(WINDY_RENDERER_D3D11_H)
/* ========================================================================
   $File: $
   $Date: $
   $Revision: $
   $Creator: Davide Stasio $
   $Notice: (C) Copyright 2020 by Davide Stasio. All Rights Reserved. $
   ======================================================================== */

struct Renderer
{
    ID3D11Device        *device;
    ID3D11DeviceContext *context;
    IDXGISwapChain      *swap_chain;
    ID3D11Texture2D     *backbuffer;

    ID3D11DepthStencilState *depth_nostencil_state;
    ID3D11DepthStencilState *nodepth_nostencil_state;
    ID3D11RenderTargetView  *render_target_rgb;
    ID3D11DepthStencilView  *render_target_depth;
};

#define WINDY_RENDERER_D3D11_H
#endif