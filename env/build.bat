@echo off

if NOT DEFINED proj_root (
call "%~dp0\shell.bat"
)

pushd "%proj_root%"

IF NOT EXIST ".\build" ( mkdir ".\build")
IF NOT EXIST ".\rundata" ( mkdir ".\rundata")
IF NOT EXIST ".\rundata\assets" ( mkdir ".\rundata\assets")

SETLOCAL

pushd ".\build"

set CommonCompilerFlags=-diagnostics:column -MTd -nologo -Gm- -GR- -EHa- -Od -Oi -WX -W4 -wd4201 -wd4100 -wd4189 -wd4505 -DWINDY_INTERNAL=1 -DWINDY_DEBUG=1 -FAsc -Z7
set CommonLinkerFlags=-incremental:no -opt:ref user32.lib
REM set RendererExports=-EXPORT:win32_load_d3d11 -EXPORT:d3d11_reload_shader -EXPORT:d3d11_load_wexp
REM user32.lib gdi32.lib winmm.lib

:: 32-bit build
REM cl %CommonCompilerFlags% "%proj_root%\code\win32_handmade.cpp" /link -subsystem:windows,5.1 %CommonLinkerFlags%

:: 64-bit build
del *.pdb > NUL 2> NUL
cl %CommonCompilerFlags% "%proj_root%\code\windy.cpp" -Fmwindy.map -LD /link -incremental:no /PDB:windy_%random%.pdb -EXPORT:WindyUpdateAndRender
cl %CommonCompilerFlags% "%proj_root%\code\win32_layer.cpp" -Fmwin32_windy.map /link %CommonLinkerFlags% d3d11.lib

popd REM .\build

pushd ".\rundata\assets"

call :compile_shader phong
call :compile_shader fonts
call :compile_shader debug
call :compile_shader background

popd REM .\rundata\assets
popd REM %proj_root
EXIT /B 0


:: Functions

:compile_shader
SETLOCAL
set name=%~1
echo(
echo Compiling shader '%name%'...
fxc -nologo -Tvs_5_0 -WX -DVERTEX_HLSL=1 -DPIXEL_HLSL=0 "%proj_root%\code\%name%.hlsl" -Fo%name%v.tmp
fxc -nologo -Tps_5_0 -WX -DVERTEX_HLSL=0 -DPIXEL_HLSL=1 "%proj_root%\code\%name%.hlsl" -Fo%name%p.tmp
move /Y "%name%v.tmp" "%name%.vsh" >nul
move /Y "%name%p.tmp" "%name%.psh" >nul
EXIT /B 0

