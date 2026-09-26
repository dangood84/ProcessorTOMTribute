@echo off
rem Build tom.exe with gcc and SDL2 on the PATH (MSYS2: pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-SDL2).
rem Copy SDL2.dll next to tom.exe if it is not already on PATH.
if not exist build mkdir build
gcc -std=c11 -Wall -Wextra -O2 -Isrc -o build\tom.exe src\main.c src\tom_machine.c src\tom_asm.c src\tom_audio.c src\tom_font.c src\tom_ui.c src\tom_test.c -lSDL2 -lm
if errorlevel 1 exit /b 1
echo Built build\tom.exe
echo Run: build\tom.exe examples\countup.tom
