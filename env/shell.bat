@echo off
set vcvars_path="C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
IF NOT EXIST %vcvars_path% (
	set vcvars_path="C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"
)
call %vcvars_path%

set DIRCMD=/o

set proj_root=%~dp0
pushd "%proj_root%\.."
set proj_root=%cd%
popd

set path="%proj_root%\env";%path%

IF NOT EXIST "%proj_root%\env\vimconfig" (
    set HOME=X:\global_vim
) ELSE (
    set HOME=%proj_root%\env\vimconfig
)
