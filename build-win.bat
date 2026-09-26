@echo off
setlocal EnableExtensions
cd /d "%~dp0"
rem SDL2.dll is only needed to start tom.exe. Compiling needs SDL.h,
rem which is in the SDL2 development files, next to gcc, not in build\.

where gcc >nul 2>&1
if errorlevel 1 (
  echo gcc was not found on PATH.
  echo Install MSYS2, open "MSYS2 MinGW x64", and run:
  echo   pacman -S --needed mingw-w64-x86_64-gcc mingw-w64-x86_64-SDL2
  exit /b 1
)

echo Compiler:
gcc -dumpmachine
for /f "delims=" %%M in ('gcc -dumpmachine') do set "MACHINE=%%M"

set "SDL_INC="
set "SDL_LIB="
set "SDL_BIN="

for /f "delims=" %%G in ('where gcc') do (
  if not defined SDL_INC (
    if exist "%%~dpG..\include\SDL2\SDL.h" (
      set "SDL_INC=%%~dpG..\include\SDL2"
      set "SDL_LIB=%%~dpG..\lib"
      set "SDL_BIN=%%~dpG"
    )
  )
)

if not defined SDL_INC if exist "C:\msys64\mingw64\include\SDL2\SDL.h" (
  set "SDL_INC=C:\msys64\mingw64\include\SDL2"
  set "SDL_LIB=C:\msys64\mingw64\lib"
  set "SDL_BIN=C:\msys64\mingw64\bin\"
)
if not defined SDL_INC if exist "C:\msys64\ucrt64\include\SDL2\SDL.h" (
  set "SDL_INC=C:\msys64\ucrt64\include\SDL2"
  set "SDL_LIB=C:\msys64\ucrt64\lib"
  set "SDL_BIN=C:\msys64\ucrt64\bin\"
)
if not defined SDL_INC if exist "C:\msys64\mingw32\include\SDL2\SDL.h" (
  set "SDL_INC=C:\msys64\mingw32\include\SDL2"
  set "SDL_LIB=C:\msys64\mingw32\lib"
  set "SDL_BIN=C:\msys64\mingw32\bin\"
)
if not defined SDL_INC if exist "%~dp0sdl2\include\SDL2\SDL.h" (
  set "SDL_INC=%~dp0sdl2\include\SDL2"
  set "SDL_LIB=%~dp0sdl2\lib"
  set "SDL_BIN=%~dp0sdl2\bin\"
)
if not defined SDL_INC if exist "%~dp0%MACHINE%\include\SDL2\SDL.h" (
  set "SDL_INC=%~dp0%MACHINE%\include\SDL2"
  set "SDL_LIB=%~dp0%MACHINE%\lib"
  set "SDL_BIN=%~dp0%MACHINE%\bin\"
)

if not defined SDL_INC (
  echo.
  echo gcc cannot find SDL.h. Putting SDL2.dll in build\ does not fix that.
  echo The DLL is used when the program starts. The compiler needs the header
  echo SDL.h, which is shipped in the SDL2 development package.
  echo.
  echo Your gcc is:
  gcc -dumpmachine
  echo.
  echo Open "MSYS2 MinGW x64" and install the matching development files:
  echo   pacman -S --needed mingw-w64-x86_64-SDL2
  echo.
  echo That puts SDL.h in the include\SDL2 folder beside gcc.
  echo Then open a new Command Prompt and run build-win.bat again.
  exit /b 1
)

echo Using SDL.h from %SDL_INC%
if not exist build mkdir build

gcc -std=c11 -Wall -Wextra -O2 -Isrc -I"%SDL_INC%" -o build\tom.exe src\main.c src\tom_machine.c src\tom_asm.c src\tom_audio.c src\tom_font.c src\tom_ui.c src\tom_test.c -L"%SDL_LIB%" -lmingw32 -lSDL2main -lSDL2 -lm -mconsole
if errorlevel 1 exit /b 1

if exist "%SDL_BIN%SDL2.dll" copy /Y "%SDL_BIN%SDL2.dll" build\ >nul
if exist "%SDL_BIN%libwinpthread-1.dll" copy /Y "%SDL_BIN%libwinpthread-1.dll" build\ >nul
if exist "%SDL_BIN%libgcc_s_seh-1.dll" copy /Y "%SDL_BIN%libgcc_s_seh-1.dll" build\ >nul
if exist "%SDL_BIN%libgcc_s_dw2-1.dll" copy /Y "%SDL_BIN%libgcc_s_dw2-1.dll" build\ >nul

echo.
echo Built build\tom.exe
echo Test:  build\tom.exe --test examples
echo Run:   run.bat
exit /b 0
