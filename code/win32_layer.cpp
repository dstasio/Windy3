/* ========================================================================
   $File: $
   $Date: $
   $Revision: $
   $Creator: Casey Muratori $
   $Notice: (C) Copyright 2014 by Molly Rocket, Inc. All Rights Reserved. $
   ======================================================================== */

#include <windows.h>
#include "windy_platform.h"
#include <d3d11.h>
//#include <d3d10.h>

#define WIDTH 1024
#define HEIGHT 720

global bool32 GlobalRunning;

LRESULT CALLBACK WindyProc(
    HWND   WindowHandle,
    UINT   Message,
    WPARAM wParam,
    LPARAM lParam
)
{
    LRESULT Result = 0;
    switch(Message)
    {
        case WM_CLOSE:
        {
            DestroyWindow(WindowHandle);
        } break;
            
        case WM_DESTROY:
        {
            GlobalRunning = false;
            PostQuitMessage(0);
        } break;

        default:
        {
          Result = DefWindowProcA(WindowHandle, Message, wParam, lParam);
        } break;
    }
    return(Result);
}


int
WinMain(
    HINSTANCE Instance,
    HINSTANCE PrevInstance,
    LPSTR CommanLine,
    int ShowFlag
)
{
    WNDCLASSA WindyClass = {};
    WindyClass.style = CS_OWNDC|CS_VREDRAW|CS_HREDRAW;
    WindyClass.lpfnWndProc = WindyProc;
    //WindyClass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    WindyClass.hInstance = Instance;
    WindyClass.lpszClassName = "WindyClass";
    ATOM Result = RegisterClassA(&WindyClass);

    RECT WindowDimensions = {0, 0, WIDTH, HEIGHT};
    AdjustWindowRect(&WindowDimensions, WS_OVERLAPPEDWINDOW, FALSE);
    WindowDimensions.right -= WindowDimensions.left;
    WindowDimensions.bottom -= WindowDimensions.top;

    HWND MainWindow = CreateWindowA("WindyClass", "Windy3",
                                    WS_OVERLAPPEDWINDOW|WS_VISIBLE,
                                    CW_USEDEFAULT, CW_USEDEFAULT,
                                    WindowDimensions.right,
                                    WindowDimensions.bottom,
                                    0, 0, Instance, 0);

    if(MainWindow)
    {
        GlobalRunning = true;

        ID3D11Device *RenderingDevice = 0;
        ID3D11DeviceContext *RenderingContext = 0;
        IDXGISwapChain *SwapChain = 0;

        DXGI_MODE_DESC DisplayModeDescriptor = {};
        //DisplayModeDescriptor.Width = WIDTH;
        //DisplayModeDescriptor.Height = HEIGHT;
        DisplayModeDescriptor.RefreshRate = {60, 1};
        DisplayModeDescriptor.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        DisplayModeDescriptor.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_PROGRESSIVE;
        DisplayModeDescriptor.Scaling = DXGI_MODE_SCALING_CENTERED;

        DXGI_SWAP_CHAIN_DESC SwapChainDescriptor = {};
        SwapChainDescriptor.BufferDesc = DisplayModeDescriptor;
        SwapChainDescriptor.SampleDesc = {1, 0};
        SwapChainDescriptor.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        SwapChainDescriptor.BufferCount = 2;
        SwapChainDescriptor.OutputWindow = MainWindow;
        SwapChainDescriptor.Windowed = true;
        SwapChainDescriptor.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
        SwapChainDescriptor.Flags;

        D3D11CreateDeviceAndSwapChain(
            0,
            D3D_DRIVER_TYPE_HARDWARE,
            0,
            D3D11_CREATE_DEVICE_BGRA_SUPPORT,
            0,
            0,
            D3D11_SDK_VERSION,
            &SwapChainDescriptor,
            &SwapChain,
            &RenderingDevice,
            0,
            &RenderingContext
        );


        ID3D11Texture2D *BackBuffer = 0;
        ID3D11RenderTargetView *TargetView = 0;
        SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void **)&BackBuffer);
        RenderingDevice->CreateRenderTargetView(BackBuffer, 0, &TargetView);
        RenderingContext->OMSetRenderTargets(0, &TargetView, 0);

        D3D11_VIEWPORT Viewport = {};
        Viewport.TopLeftX = 0;
        Viewport.TopLeftY = 0;
        Viewport.Width = WIDTH;
        Viewport.Height = HEIGHT;
        RenderingContext->RSSetViewports(1, &Viewport);

        MSG Message = {};
        while(GlobalRunning)
        {
            BOOL MessageStatus = PeekMessageA(&Message, MainWindow, 0, 0, PM_REMOVE);
            if (MessageStatus > 0)
            {
                TranslateMessage(&Message);
                DispatchMessage(&Message);
            }
            else if (MessageStatus == -1)
            {
                return 1;
            }


            real32 ClearColor[] = {1.f, 0.f, 1.f, 1.f};
            RenderingContext->ClearRenderTargetView(TargetView, ClearColor);

            SwapChain->Present(0, 0);
        }
    }
    else
    {
        // TODO: Logging
        return 1;
    }

    return 0;
}
