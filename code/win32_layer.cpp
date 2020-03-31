/* ========================================================================
   $File: $
   $Date: $
   $Revision: $
   $Creator: Davide Stasio $
   $Notice: (C) Copyright 2014 by Davide Stasio. All Rights Reserved. $
   ======================================================================== */

#include <windows.h>
#include <d3d11.h>
#include <dxgidebug.h>
#include <dxgidebug.h>

#include "windy.h"
#include "windy_platform.h"

#include "win32_layer.h"
#include <stdio.h>
//#include <d3d10.h>

#if WINDY_INTERNAL
#define OutputString(s, ...) {char Buffer[100];sprintf_s(Buffer, s, __VA_ARGS__);OutputDebugStringA(Buffer);}
#define ThrowErrorAndExit(e, ...) {OutputString("[ERROR] " ## e, __VA_ARGS__); getchar(); GlobalError = true;}
#define ThrowError(e, ...) OutputString("[ERROR] " ## e, __VA_ARGS__)
#define Warn(w, ...) OutputString("[WARNING] " ## w, __VA_ARGS__)
#define Info(i, ...) OutputString("[INFO] " ## i, __VA_ARGS__)
#else
#define OutputString(s, ...)
#define ThrowErrorAndExit(e, ...)
#define ThrowError(e, ...)
#define Warn(w, ...)
#define Info(i, ...)
#endif
#define KeyDown(Code, Key) {if(Message.wParam == (Code))  NewInput->Held.Key = 1;}
#define KeyUp(Code, Key)   {if(Message.wParam == (Code)) {NewInput->Held.Key = 0;NewInput->Pressed.Key = 1;}}

#ifndef MAX_PATH
#define MAX_PATH 100
#endif

global b32 GlobalRunning;
global b32 GlobalError;

internal
PLATFORM_READ_FILE(Win32ReadFile)
{
    file Result = {};
    HANDLE FileHandle = CreateFileA(Path, GENERIC_READ, FILE_SHARE_READ, 0,
                                    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

    if (FileHandle != INVALID_HANDLE_VALUE)
    {
        // TODO(dave): Currently only supports up to 4GB files
        u32 FileSize = GetFileSize(FileHandle, 0);
        DWORD BytesRead;

        // TODO(dave): Remove this allocation
        u8 *Buffer = (u8 *)VirtualAlloc(0, FileSize, MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE);
        if(ReadFile(FileHandle, Buffer, FileSize, &BytesRead, 0))
        {
            Result.Data = Buffer;
            Result.Size = (u32)BytesRead;
        }
        else
        {
            ThrowError("Unable to read file: %s\n", Path);
        }

        CloseHandle(FileHandle);
    }
    else
    {
        ThrowError("Unable to open file: %s\n", Path);
    }

    return(Result);
}

inline FILETIME
GetLastWriteTime(char *Path)
{
    WIN32_FILE_ATTRIBUTE_DATA FileAttribs = {};
    GetFileAttributesExA(Path, GetFileExInfoStandard, (void *)&FileAttribs);

    return(FileAttribs.ftLastWriteTime);
}

// NOTE(dave): This requires char arrays of length 'MAX_PATH'
internal void
GetWindyPaths(char *OriginalPath, char *TempPath)
{
    u32 PathLength = GetModuleFileNameA(0, OriginalPath, MAX_PATH);

    char *c = OriginalPath + PathLength;
    while(*c != '\\') {c--; PathLength--;}

    char *DLLName = "windy.dll";

    while(*DLLName != '\0')
    {
        *(++c) = *(DLLName++);
        PathLength++;
    }
    OriginalPath[++PathLength] = '\0';

    for(u32 i = 0;
        i <= PathLength;
        ++i)
    {
        TempPath[i] = OriginalPath[i];
    }
    TempPath[PathLength] = '0';
    TempPath[PathLength+1] = '\0';
}

internal win32_game_code
LoadWindy()
{
    win32_game_code GameCode = {};
    GameCode.DLL = LoadLibraryA("windy.dll0");
    if(GameCode.DLL)
    {
        GameCode.GameUpdateAndRender = (game_update_and_render *)GetProcAddress(GameCode.DLL, "WindyUpdateAndRender");
    }
    else
    {
        ThrowErrorAndExit("Could not load windy.dll0");
    }

    return(GameCode);
}

internal void
UnloadWindy(win32_game_code *GameCode)
{
    GameCode->GameUpdateAndRender = 0;
    FreeLibrary(GameCode->DLL);
}

internal void
CheckAndReloadWindy(win32_game_code *GameCode)
{
    char OriginalPath[MAX_PATH];
    char TempPath[MAX_PATH];
    GetWindyPaths(OriginalPath, TempPath);

    FILETIME CurrentWriteTime = GetLastWriteTime(OriginalPath);

    if(CompareFileTime(&CurrentWriteTime, &GameCode->WriteTime))
    {
        UnloadWindy(GameCode);

        while(!CopyFile(OriginalPath, TempPath, false)) {}
        *GameCode = LoadWindy();
        GameCode->WriteTime = CurrentWriteTime;
    }
}

internal b32
CheckAndReloadShader(char *Path, shader *Shader)
{
    b32 HasChanged = false;
    FILETIME CurrentWriteTime = GetLastWriteTime(Path);

    if(CompareFileTime(&CurrentWriteTime, &Shader->WriteTime))
    {
        VirtualFree(Shader->Bytes.Data, 0, MEM_RELEASE);
        Shader->Bytes = Win32ReadFile(Path);
        Shader->WriteTime = CurrentWriteTime;
        HasChanged = true;
    }

    return(HasChanged);
}

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
    // @todo add support for low-resolution performance counters
    // @todo maybe add support for 32-bit
    i64 performance_counter_frequency = 0;
    Assert(QueryPerformanceFrequency((LARGE_INTEGER *)&performance_counter_frequency));


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
#if WINDY_INTERNAL
        LPVOID BaseAddress = (LPVOID)Terabytes(2);
#else
        LPVOID BaseAddress = 0;
#endif
        GlobalRunning = true;
        GlobalError = false;
        game_memory GameMemory = {};
        GameMemory.StorageSize = Megabytes(500);
        GameMemory.Storage = VirtualAlloc(BaseAddress, GameMemory.StorageSize,
                                           MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE);
        GameMemory.ReadFile = Win32ReadFile;

        win32_game_code Windy = LoadWindy();

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
            D3D11_CREATE_DEVICE_BGRA_SUPPORT|D3D11_CREATE_DEVICE_DEBUG,
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

        D3D11_VIEWPORT Viewport = {};
        Viewport.TopLeftX = 0;
        Viewport.TopLeftY = 0;
        Viewport.Width = WIDTH;
        Viewport.Height = HEIGHT;
        RenderingContext->RSSetViewports(1, &Viewport);

        //
        // shader initialization
        //
        // TODO(dave): Include all this in GameState struct
        shader VSRaw = {};
        shader PSRaw = {};
        ID3D11VertexShader *VSLinked;
        ID3D11PixelShader *PSLinked;
        CheckAndReloadShader("assets\\vs.sh", &VSRaw);
        RenderingDevice->CreateVertexShader(VSRaw.Bytes.Data, VSRaw.Bytes.Size,
                                            0, &VSLinked);
        CheckAndReloadShader("assets\\ps.sh", &PSRaw);
        RenderingDevice->CreatePixelShader(PSRaw.Bytes.Data, PSRaw.Bytes.Size,
                                           0, &PSLinked);
        RenderingContext->VSSetShader(VSLinked, 0, 0);
        RenderingContext->PSSetShader(PSLinked, 0, 0);

        input Inputs[1] = {};

        input *NewInput = &Inputs[0];
        //input *OldInput = &Inputs[1];
        MSG Message = {};
        u32 Count = 0;

        i64 last_performance_counter = 0;
        i64 current_performance_counter = 0;
        Assert(QueryPerformanceCounter((LARGE_INTEGER *)&last_performance_counter));
        while(GlobalRunning && !GlobalError && !NewInput->Pressed.esc)
        {
            NewInput->Pressed = {};
            while(PeekMessageA(&Message, MainWindow, 0, 0, PM_REMOVE))
            {
                switch(Message.message)
                {
                    case WM_KEYDOWN:
                    {
                        KeyDown(VK_UP,     up);
                        KeyDown(VK_DOWN,   down);
                        KeyDown(VK_LEFT,   left);
                        KeyDown(VK_RIGHT,  right);
                        KeyDown(VK_W,      w);
                        KeyDown(VK_A,      a);
                        KeyDown(VK_S,      s);
                        KeyDown(VK_D,      d);
                        KeyDown(VK_ESCAPE, esc);
                    } break;

                    case WM_KEYUP:
                    {
                        KeyUp(VK_UP,     up);
                        KeyUp(VK_DOWN,   down);
                        KeyUp(VK_LEFT,   left);
                        KeyUp(VK_RIGHT,  right);
                        KeyUp(VK_W,      w);
                        KeyUp(VK_A,      a);
                        KeyUp(VK_S,      s);
                        KeyUp(VK_D,      d);
                        KeyUp(VK_ESCAPE, esc);
                    } break;

                    default:
                    {
                        TranslateMessage(&Message);
                        DispatchMessage(&Message);
                    } break;
                }
            }

#if WINDY_INTERNAL
            CheckAndReloadWindy(&Windy);

            if(CheckAndReloadShader("assets\\vs.sh", &VSRaw))
            {
                // TODO(dave) maybe change if to while
                while(VSRaw.Bytes.Size == 0)
                {
                    VSRaw.WriteTime = {};
                    CheckAndReloadShader("assets\\vs.sh", &VSRaw);
                }
                VSLinked->Release();
                RenderingDevice->CreateVertexShader(VSRaw.Bytes.Data, VSRaw.Bytes.Size,
                                                    0, &VSLinked);
                RenderingContext->VSSetShader(VSLinked, 0, 0);

                //Assert(ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_SUMMARY) == S_OK);
            }
            if(CheckAndReloadShader("assets\\ps.sh", &PSRaw))
            {
                while(PSRaw.Bytes.Size == 0)
                {
                    PSRaw.WriteTime = {};
                    CheckAndReloadShader("assets\\ps.sh", &PSRaw);
                }
                PSLinked->Release();
                RenderingDevice->CreatePixelShader(PSRaw.Bytes.Data, PSRaw.Bytes.Size,
                                                   0, &PSLinked);
                RenderingContext->PSSetShader(PSLinked, 0, 0);

                //Assert(ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_SUMMARY) == S_OK);
            }
#endif

            Assert(QueryPerformanceCounter((LARGE_INTEGER *)&current_performance_counter));
            r32 dtime = (r32)(current_performance_counter - last_performance_counter) / (r32)performance_counter_frequency;
            if(Windy.GameUpdateAndRender)
            {
                Windy.GameUpdateAndRender(NewInput, dtime, RenderingDevice, RenderingContext,
                                          TargetView, VSRaw.Bytes, &GameMemory);
            }
            last_performance_counter = current_performance_counter;
            Info("Frametime: %f     FPS:%d\n", dtime, (u32)(1/dtime));

//            input *SwitchInput = NewInput;
//            NewInput = OldInput;
//            OldInput = SwitchInput;

            SwapChain->Present(0, 0);
        }
    }
    else
    {
        // TODO: Logging
        return 1;
    }

    if (GlobalError)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}
