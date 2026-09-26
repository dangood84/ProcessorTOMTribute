@echo off
cd /d "%~dp0"
call "%~dp0build-win.bat"
if errorlevel 1 exit /b 1
build\tom.exe examples\countup.tom
