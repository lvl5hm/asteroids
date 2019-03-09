@echo off

pushd build

set compilerFlags=-O2 -DASTEROIDS_PROD -Gm -MTd -nologo -Oi -GR- -EHa- -WX -W4 -wd4702 -wd4005 -wd4505 -wd4456 -wd4201 -wd4100 -wd4189 -Zi -FC
set linkerFlags=-incremental:no -opt:ref user32.lib gdi32.lib winmm.lib OpenGL32.lib Xaudio2.lib

cl %compilerFlags% ..\code\win32_main.cpp /link %linkerFlags%

popd