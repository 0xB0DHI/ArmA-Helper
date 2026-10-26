@echo off
setlocal
cd /d "%~dp0"
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSROOT=%%i"
if not defined VSROOT exit /b 1
call "%VSROOT%\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64
if errorlevel 1 exit /b 1
if not exist out\user\imgui mkdir out\user\imgui
cl /nologo /std:c++17 /EHsc /W4 /WX /c /Ithird_party\imgui /Ithird_party\imgui\backends /Foout\user\imgui\ third_party\imgui\imgui.cpp third_party\imgui\imgui_draw.cpp third_party\imgui\imgui_tables.cpp third_party\imgui\imgui_widgets.cpp third_party\imgui\backends\imgui_impl_win32.cpp third_party\imgui\backends\imgui_impl_dx11.cpp
if errorlevel 1 exit /b 1
set "IMGUI_OBJS=out\user\imgui\imgui.obj out\user\imgui\imgui_draw.obj out\user\imgui\imgui_tables.obj out\user\imgui\imgui_widgets.obj out\user\imgui\imgui_impl_win32.obj out\user\imgui\imgui_impl_dx11.obj"
cl /nologo /std:c++17 /EHsc /W4 /WX /Zi /Ithird_party\imgui /Ithird_party\imgui\backends /Fo:out\user\controller.obj /Fd:out\user\controller-compile.pdb /Fe:out\user\controller.exe controller.cpp %IMGUI_OBJS% /link user32.lib gdi32.lib d3d11.lib dxgi.lib dcomp.lib dwmapi.lib /SUBSYSTEM:WINDOWS /DEBUG /PDB:out\user\controller.pdb
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /EHsc /W4 /WX /Ithird_party\imgui /Ithird_party\imgui\backends /Fo:out\user\controller-read-tests.obj /Fe:out\user\controller-read-tests.exe controller-read-tests.cpp %IMGUI_OBJS% /link user32.lib gdi32.lib d3d11.lib dxgi.lib dcomp.lib dwmapi.lib /SUBSYSTEM:CONSOLE
exit /b %errorlevel%
