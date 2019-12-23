/* ========================================================================
   $File: $
   $Date: $
   $Revision: $
   $Creator: Casey Muratori $
   $Notice: (C) Copyright 2014 by Molly Rocket, Inc. All Rights Reserved. $
   ======================================================================== */

#include <windows.h>
#include "windy_platform.h"

global bool32 GlobalRunning;

LRESULT CALLBACK WindyProc(
    HWND   WindowHandle,
    UINT   Message,
    WPARAM wParam,
    LPARAM lParam
)
{
    switch(Message)
    {
        case WM_DESTROY:
        {
            GlobalRunning = false;
        } break;

        default:
        {
          return DefWindowProcA(WindowHandle, Message, wParam, lParam);
        } break;
    }
    return 0;
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
    WindyClass.hInstance = Instance;
    WindyClass.lpszClassName = "WindyClass";
    ATOM Result = RegisterClassA(&WindyClass);

    HWND MainWindow = CreateWindowA("WindyClass", "Windy3",
                                         WS_OVERLAPPEDWINDOW|WS_VISIBLE,
                                         CW_USEDEFAULT, CW_USEDEFAULT,
                                         1024, 720,
                                         0, 0,
                                         Instance, 0);

    if(MainWindow)
    {
        GlobalRunning = true;
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
        }
    }
    else
    {
        // TODO: Logging
        return 1;
    }

    return 0;
}
