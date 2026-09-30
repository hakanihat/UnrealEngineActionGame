@echo off
setlocal
echo.
echo  Copies the ActionGame code into a fresh Unreal C++ project named "ActionGame".
echo  (Close the Unreal Editor and Visual Studio for that project first.)
echo.
set /p "TARGET=Drag the NEW project's folder here (the one with ActionGame.uproject) and press Enter: "
set "TARGET=%TARGET:"=%"
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0Tools\CopyToNewProject.ps1" -NewProjectDir "%TARGET%"
echo.
pause
