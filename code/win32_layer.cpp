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
#include <xinput.h>

#include "windy.h"
#include "windy_platform.h"

#include "win32_layer.h"
#include <stdio.h>
//#include <d3d10.h>

#if WINDY_INTERNAL
#define __INTERNAL_DISABLE_WARNINGS  _Pragma("warning(push)") \
                                     _Pragma("warning(disable : 456)") /* -> declaration of 'identifier' hides previous local declaration */
#define __INTERNAL_RESET_WARNINGS    _Pragma("warning(pop)")

#define output_string(s, ...)        {__INTERNAL_DISABLE_WARNINGS char Buffer[100];sprintf_s(Buffer, s, __VA_ARGS__);OutputDebugStringA(Buffer); __INTERNAL_RESET_WARNINGS}
#define throw_error_and_exit(e, ...) {output_string(" ------------------------------[ERROR] " ## e, __VA_ARGS__); getchar(); global_error = true;}
#define throw_error(e, ...)           output_string(" ------------------------------[ERROR] " ## e, __VA_ARGS__)
#define inform(i, ...)                output_string(" ------------------------------[INFO] " ## i, __VA_ARGS__)
#define warn(w, ...)                  output_string(" ------------------------------[WARNING] " ## w, __VA_ARGS__)
#else
#define output_string(s, ...)
#define throw_error_and_exit(e, ...)
#define throw_error(e, ...)
#define inform(i, ...)
#define warn(w, ...)
#endif
#define key_down(code, key)    {if(Message.wParam == (code))  input.held.key = 1;}
#define key_up(code, key)      {if(Message.wParam == (code)) {input.held.key = 0;input.pressed.key = 1;}}
#define raw_mouse_button(id, key)  {if(raw_mouse.usButtonFlags & RI_MOUSE_BUTTON_ ## id ##_DOWN)  input.held.key = 1; else if (raw_mouse.usButtonFlags & RI_MOUSE_BUTTON_ ## id ##_UP) {input.held.key = 0;input.pressed.key = 1;}}
#define mouse_down(id, key)    
#define mouse_up(id, key)      
#define file_time_to_u64(wt) ((wt).dwLowDateTime | ((u64)((wt).dwHighDateTime) << 32))

// @todo, @critical: Use VirtualProtect (handmade_hero day 004) to check for freed-memory access errors.
// @todo, @qol:      Xinput dynamic loading.

#include "config.h"

global b32 global_running;
global b32 global_error;
global b32 global_window_is_active;
global Platform_Renderer *global_renderer;

inline u64
win32_get_last_write_time(char *Path)
{
    WIN32_FILE_ATTRIBUTE_DATA FileAttribs = {};
    GetFileAttributesExA(Path, GetFileExInfoStandard, (void *)&FileAttribs);
    FILETIME last_write_time = FileAttribs.ftLastWriteTime;

    u64 result = file_time_to_u64(last_write_time);
    return result;
}

internal
PLATFORM_READ_FILE(win32_read_file)
{
    Input_File Result = {};
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
            Result.data = Buffer;
            Result.size = (u32)BytesRead;
            Result.path = Path;
            Result.write_time = win32_get_last_write_time(Path);
        }
        else
        {
            throw_error("Unable to read file: %s\n", Path);
        }

        CloseHandle(FileHandle);
    }
    else
    {
        throw_error("Unable to open file: %s\n", Path);
        DWORD error = GetLastError();
        error = 0;
    }

    return(Result);
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

internal Win32_Game_Code
load_windy()
{
    char orig_path[MAX_PATH];
    char temp_path[MAX_PATH];
    GetWindyPaths(orig_path, temp_path);
    while(!CopyFile(orig_path, temp_path, false)) {}

    Win32_Game_Code game_code = {};
    game_code.dll = LoadLibraryA("windy.dll0");
    if(game_code.dll)
    {
        game_code.game_update_and_render = (Game_Update_And_Render *)GetProcAddress(game_code.dll, "WindyUpdateAndRender");
    }
    else
    {
        throw_error_and_exit("Could not load 'windy.dll0'\n");
    }

    return(game_code);
}

internal void
UnloadWindy(Win32_Game_Code *game_code)
{
    game_code->game_update_and_render = 0;
    FreeLibrary(game_code->dll);
}

internal void
reload_windy(Win32_Game_Code *game_code)
{
    char orig_path[MAX_PATH];
    char temp_path[MAX_PATH];
    GetWindyPaths(orig_path, temp_path);
    u64 current_write_time = win32_get_last_write_time(orig_path);

    if(current_write_time != game_code->write_time)
    {
        UnloadWindy(game_code);

        *game_code = load_windy();
        game_code->write_time = current_write_time;
    }
}

internal 
PLATFORM_RELOAD_CHANGED_FILE(win32_reload_file_if_changed)
{
    b32 has_changed = false;
    u64 current_write_time = win32_get_last_write_time(file->path);

    Input_File new_file = {};
    if(current_write_time != file->write_time)
    {
        u32 trial_count = 0;
        while(!new_file.size)
        {
            VirtualFree(new_file.data, 0, MEM_RELEASE);
            new_file = win32_read_file(file->path);
            new_file.write_time = current_write_time;
            has_changed = true;

            if (trial_count++ >= 100) // Try to read until it succeeds, or until trial count reaches max.
            {
                has_changed = false;
                throw_error_and_exit("Couldn't read changed file '%s'!", file->path);
                break;
            }
        }

        if (has_changed) {
            VirtualFree(file->data, 0, MEM_RELEASE);
            *file = new_file;
        }
    }

    return(has_changed);
}

internal 
PLATFORM_CLOSE_FILE(win32_close_file)
{
    if ((file) &&
        (file->data))
    {
        VirtualFree(file->data, 0, MEM_RELEASE);
    }
}

#include "win32_renderer_d3d11.cpp"

LRESULT CALLBACK WindyProc(
    HWND   WindowHandle,
    UINT   Message,
    WPARAM w,
    LPARAM l
)
{
    LRESULT Result = 0;
    switch(Message)
    {
        case WM_ACTIVATEAPP:
        {
            global_window_is_active = w == TRUE;
            if (w == TRUE) {
                inform("activate\n");
            }
            else {
                inform("deactivate\n");
            }
        } break;

        case WM_CLOSE:
        {
            DestroyWindow(WindowHandle);
        } break;

        case WM_DESTROY:
        {
            global_running = false;
            PostQuitMessage(0);
        } break;

        case WM_SIZE:
        {
            global_width = LOWORD(l);
            global_height = HIWORD(l);

            if (global_renderer)  d3d11_resize_render_targets();
        } break;

        // @todo: implement this
        case WM_SETCURSOR:
        {
        } break;

        default:
        {
            Result = DefWindowProcA(WindowHandle, Message, w, l);
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
    s64 performance_counter_frequency = 0;
    Assert(QueryPerformanceFrequency((LARGE_INTEGER *)&performance_counter_frequency));
    // @todo make this hardware-dependant
    r32 target_ms_per_frame = 1.f/60.f;

    WNDCLASSA WindyClass = {};
    WindyClass.style = CS_OWNDC|CS_VREDRAW|CS_HREDRAW;
    WindyClass.lpfnWndProc = WindyProc;
    //WindyClass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    WindyClass.hInstance = Instance;
    WindyClass.lpszClassName = "WindyClass";
    ATOM Result = RegisterClassA(&WindyClass);

    RECT WindowDimensions = {0, 0, (s32)global_width, (s32)global_height};
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
        ShowCursor(false);
        RECT window_rect = {};
        GetWindowRect(MainWindow, &window_rect);
        u32 window_width  = window_rect.right - window_rect.left;
        u32 window_height = window_rect.bottom - window_rect.top;

        //
        // raw input set-up
        //
        RAWINPUTDEVICE raw_in_devices[] = {
            {0x01, 0x02, 0, MainWindow}
        };
        RegisterRawInputDevices(raw_in_devices, 1, sizeof(raw_in_devices[0]));

#if WINDY_INTERNAL
        LPVOID base_address = (LPVOID)Terabytes(2);
#else
        LPVOID base_address = 0;
#endif
        global_running = true;
        global_error = false;
        game_memory GameMemory = {};
        // @todo: assert storage allocations
        GameMemory.main_storage_size = Megabytes(50);
        GameMemory.main_storage = VirtualAlloc(base_address, GameMemory.main_storage_size,
                                               MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE);
        GameMemory.volatile_storage_size = Gigabytes(1);
        GameMemory.volatile_storage = VirtualAlloc(byte_offset(base_address, GameMemory.main_storage_size),
                                                   GameMemory.volatile_storage_size,
                                                   MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE);
        GameMemory.read_file  = win32_read_file;
        GameMemory.close_file = win32_close_file;
        GameMemory.reload_if_changed = win32_reload_file_if_changed;

        Win32_Game_Code windy = load_windy();

        // @todo: probably a global variable is a good idea?
        Platform_Renderer renderer = {};
        D11_Renderer d11 = {};
        renderer.load_renderer       = win32_load_d3d11;
        renderer.reload_shader       = d3d11_reload_shader;
        renderer.init_texture        = d3d11_init_texture;
        renderer.init_mesh           = d3d11_init_mesh;
        renderer.init_square_mesh    = d3d11_init_square_mesh; // @todo: this is to be removed; called in a general init function for the renderer
        renderer.clear               = d3d11_clear;
        renderer.set_active_mesh     = d3d11_set_active_mesh;
        renderer.set_active_texture  = d3d11_set_active_texture;
        renderer.set_active_shader   = d3d11_set_active_shader;
        renderer.set_render_targets  = d3d11_set_default_render_targets;
        renderer.set_depth_stencil   = d3d11_set_depth_stencil;
        renderer.draw_rect             = d3d11_draw_rect;
        renderer.draw_text             = d3d11_draw_text;
        renderer.draw_mesh             = d3d11_draw_mesh;
        renderer.draw_line             = d3d11_draw_line;
        renderer.internal_sandbox_call = d3d11_internal_sandbox_call;
        renderer.platform = (void *)&d11;
        global_renderer = &renderer;

        DXGI_MODE_DESC display_mode_desc = {};
        //DisplayModeDescriptor.Width = global_width;
        //DisplayModeDescriptor.Height = global_height;
        display_mode_desc.RefreshRate = {60, 1};
        display_mode_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        display_mode_desc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_PROGRESSIVE;
        display_mode_desc.Scaling = DXGI_MODE_SCALING_CENTERED;

        DXGI_SWAP_CHAIN_DESC swap_chain_desc = {};
        swap_chain_desc.BufferDesc = display_mode_desc;
        swap_chain_desc.SampleDesc = {1, 0};
        swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swap_chain_desc.BufferCount = 2;
        swap_chain_desc.OutputWindow = MainWindow;
        swap_chain_desc.Windowed = true;
        swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
        swap_chain_desc.Flags;

        D3D11CreateDeviceAndSwapChain(
            0,
            D3D_DRIVER_TYPE_HARDWARE,
            0,
            D3D11_CREATE_DEVICE_BGRA_SUPPORT|D3D11_CREATE_DEVICE_DEBUG,
            0,
            0,
            D3D11_SDK_VERSION,
            &swap_chain_desc,
            &d11.swap_chain,
            &d11.device,
            0,
            &d11.context
        );

        Input input = {};
        MSG Message = {};

        s64 last_performance_counter = 0;
        s64 current_performance_counter = 0;
        Assert(QueryPerformanceCounter((LARGE_INTEGER *)&last_performance_counter));
        Gamemode gamemode = GAMEMODE_GAME;
        while(global_running && !global_error)
        {
            input.pressed = {};
            input.mouse.dp = {};
            input.mouse.wheel = 0;

            if (!global_window_is_active) input.held = {};

            Assert(QueryPerformanceCounter((LARGE_INTEGER *)&current_performance_counter));
            r32 dtime = (r32)(current_performance_counter - last_performance_counter) / (r32)performance_counter_frequency;

            while(dtime <= target_ms_per_frame)
            {
                while(PeekMessageA(&Message, MainWindow, 0, 0, PM_REMOVE))
                {
                    switch(Message.message)
                    {
                        case WM_SYSKEYDOWN:
                        case WM_KEYDOWN:
                        {
                            key_down(VK_UP,      up);
                            key_down(VK_DOWN,    down);
                            key_down(VK_LEFT,    left);
                            key_down(VK_RIGHT,   right);
                            key_down(VK_W,       w);
                            key_down(VK_A,       a);
                            key_down(VK_S,       s);
                            key_down(VK_D,       d);
                            key_down(VK_E,       e);
                            key_down(VK_F,       f);
                            key_down(VK_G,       g);
                            key_down(VK_Q,       q);
                            key_down(VK_X,       x);
                            key_down(VK_Y,       y);
                            key_down(VK_Z,       z);
                            key_down(VK_SPACE,   space);
                            key_down(VK_SHIFT,   shift);
                            key_down(VK_CONTROL, ctrl);
                            key_down(VK_ESCAPE,  esc);
                            key_down(VK_TAB,     tab);
                            key_down(VK_MENU,    alt);
                        } break;

                        case WM_SYSKEYUP:
                        case WM_KEYUP:
                        {
                            key_up(VK_UP,      up);
                            key_up(VK_DOWN,    down);
                            key_up(VK_LEFT,    left);
                            key_up(VK_RIGHT,   right);
                            key_up(VK_W,       w);
                            key_up(VK_A,       a);
                            key_up(VK_S,       s);
                            key_up(VK_D,       d);
                            key_up(VK_E,       e);
                            key_up(VK_F,       f);
                            key_up(VK_G,       g);
                            key_up(VK_Q,       q);
                            key_up(VK_X,       x);
                            key_up(VK_Y,       y);
                            key_up(VK_Z,       z);
                            key_up(VK_SPACE,   space);
                            key_up(VK_SHIFT,   shift);
                            key_up(VK_CONTROL, ctrl);
                            key_up(VK_ESCAPE,  esc);
                            key_up(VK_TAB,     tab);
                            key_up(VK_MENU,    alt);
                        } break;

#define WINDY_WIN32_MOUSE_SENSITIVITY 1000.f
                        case WM_INPUT:
                        {
                            RAWINPUT raw_input = {};
                            u32 raw_size = sizeof(raw_input);
                            GetRawInputData((HRAWINPUT)Message.lParam, RID_INPUT, &raw_input, &raw_size, sizeof(RAWINPUTHEADER));
                            RAWMOUSE raw_mouse = raw_input.data.mouse;
                            if ((raw_mouse.usFlags & MOUSE_MOVE_RELATIVE) == MOUSE_MOVE_RELATIVE)
                            {
                                input.mouse.dx =  ((r32)raw_mouse.lLastX / 65535.f)*WINDY_WIN32_MOUSE_SENSITIVITY;
                                input.mouse.dy = -((r32)raw_mouse.lLastY / 65535.f)*WINDY_WIN32_MOUSE_SENSITIVITY;
                            }

                            raw_mouse_button(1, mouse_left);
                            raw_mouse_button(2, mouse_right);
                            raw_mouse_button(3, mouse_middle);

                            if (raw_mouse.usButtonFlags & RI_MOUSE_WHEEL)
                            {
                                input.mouse.wheel = raw_mouse.usButtonData;
                            }

                            if (gamemode == GAMEMODE_GAME)
                            {
                                SetCursorPos((window_rect.right - window_rect.left)/2, (window_rect.bottom - window_rect.top)/2);
                            }
                        } break;

                        case WM_MOUSEMOVE:
                        {
                            if (gamemode == GAMEMODE_EDITOR)
                            {
                                // @todo: doing the division here might cause problems if a window resize
                                //        happens between frames.
                                input.mouse.x = (r32)(((s16*)&Message.lParam)[0]) / global_width;
                                input.mouse.y = (r32)(((s16*)&Message.lParam)[1]) / global_height;
                            }
                        } break;

                        default:
                        {
                            TranslateMessage(&Message);
                            DispatchMessage(&Message);
                        } break;
                    }
                    if (input.pressed.tab)
                    {
                        if (gamemode == GAMEMODE_GAME)
                        {
                            gamemode = GAMEMODE_EDITOR;
                            ShowCursor(true);
                        }
                        else if (gamemode == GAMEMODE_EDITOR)
                        {
                            gamemode = GAMEMODE_GAME;
                            ShowCursor(false);
                        }
                    }
                    if (input.pressed.esc)
                    {
                        global_running = false;
                        return 0;
                    }
                }

                // Getting controller input
                XINPUT_STATE controller_state = {};
                if (XInputGetState(0, &controller_state) == ERROR_SUCCESS)
                {
                    input.gamepad.start        = 0 != (controller_state.Gamepad.wButtons & XINPUT_GAMEPAD_START);
                    input.gamepad.select       = 0 != (controller_state.Gamepad.wButtons & XINPUT_GAMEPAD_BACK);
                    input.gamepad.dpad_up      = 0 != (controller_state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP);
                    input.gamepad.dpad_down    = 0 != (controller_state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
                    input.gamepad.dpad_left    = 0 != (controller_state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
                    input.gamepad.dpad_right   = 0 != (controller_state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);
                    input.gamepad.action_up    = 0 != (controller_state.Gamepad.wButtons & XINPUT_GAMEPAD_Y);
                    input.gamepad.action_down  = 0 != (controller_state.Gamepad.wButtons & XINPUT_GAMEPAD_A);
                    input.gamepad.action_left  = 0 != (controller_state.Gamepad.wButtons & XINPUT_GAMEPAD_X);
                    input.gamepad.action_right = 0 != (controller_state.Gamepad.wButtons & XINPUT_GAMEPAD_B);
                    input.gamepad.thumb_left   = 0 != (controller_state.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_THUMB);
                    input.gamepad.thumb_right  = 0 != (controller_state.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB);

                    input.gamepad.trigger_left   = ((r32)controller_state.Gamepad. bLeftTrigger) / 255.f;
                    input.gamepad.trigger_right  = ((r32)controller_state.Gamepad.bRightTrigger) / 255.f;

                    input.gamepad.stick_left_dir  = { (r32)controller_state.Gamepad.sThumbLX, (r32)controller_state.Gamepad.sThumbLY };
                    r32 left_stick_magnitude      = length(input.gamepad.stick_left_dir);
                    if (left_stick_magnitude > XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) {
                        input.gamepad.stick_left_dir /= left_stick_magnitude;

                        if (left_stick_magnitude > 32767.f) left_stick_magnitude = 32767.f;
                        left_stick_magnitude -= XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE;

                        input.gamepad.stick_left_magnitude = left_stick_magnitude / (32767.f - (r32)XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
                    }
                    else {
                        input.gamepad.stick_left_dir       = {};
                        input.gamepad.stick_left_magnitude = 0.f;
                    }

                    input.gamepad.stick_right_dir  = { (r32)controller_state.Gamepad.sThumbRX, (r32)controller_state.Gamepad.sThumbRY };
                    r32 right_stick_magnitude      = length(input.gamepad.stick_right_dir);
                    if (right_stick_magnitude > XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE) {
                        input.gamepad.stick_right_dir /= right_stick_magnitude;

                        if (right_stick_magnitude > 32767.f) right_stick_magnitude = 32767.f;
                        right_stick_magnitude -= XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE;

                        input.gamepad.stick_right_magnitude = right_stick_magnitude / (32767.f - (r32)XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);
                    }
                    else {
                        input.gamepad.stick_right_dir       = {};
                        input.gamepad.stick_right_magnitude = 0.f;
                    }
                }
                else 
                {
                }

                Assert(QueryPerformanceCounter((LARGE_INTEGER *)&current_performance_counter));
                dtime = (r32)(current_performance_counter - last_performance_counter) / (r32)performance_counter_frequency;
            }

#if WINDY_INTERNAL
            reload_windy(&windy);
#endif

            if(windy.game_update_and_render)
            {
                windy.game_update_and_render(&input, dtime, &renderer, &GameMemory, &gamemode, global_width, global_height);
            }
            else
            {
                Assert(0);
                // @todo: report error
            }
            last_performance_counter = current_performance_counter;

            d11.swap_chain->Present(0, 0);
        }
    }
    else
    {
        // TODO: Logging
        return 1;
    }

    if (global_error)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}
