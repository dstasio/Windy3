REM TODO - can we just build both with one exe?

set CommonCompilerFlags=-MTd -nologo -Gm- -GR- -EHa- -Od -Oi -WX -W4 -wd4201 -wd4100 -wd4189 -wd4456 -wd4505 -DHANDMADE_INTERNAL=1 -FAsc -Z7
set CommonLinkerFlags=-incremental:no -opt:ref 
REM user32.lib gdi32.lib winmm.lib

IF NOT EXIST ..\build mkdir ..\build
pushd ..\build

REM 32-bit build
REM cl %CommonCompilerFlags% ..\code\win32_handmade.cpp /link -subsystem:windows,5.1 %CommonLinkerFlags%

REM 64-bit build
del *.pdb > NUL 2> NUL
REM cl %CommonCompilerFlags% ..\code\windy.cpp -Fmwindy.map -LD /link -incremental:no /PDB:windy_%random%.pdb -EXPORT:GameGetSoundSamples -EXPORT:GameUpdateAndRender
cl %CommonCompilerFlags% ..\code\win32_layer.cpp -Fmwin32_windy.map /link %CommonLinkerFlags%
popd

