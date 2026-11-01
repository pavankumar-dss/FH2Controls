@echo off
rem Builds build\FH2Controls.dll with the Visual Studio Build Tools (x64, static CRT).
setlocal
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>nul || exit /b 1
cd /d "%~dp0"
if not exist build mkdir build
cl /nologo /std:c++17 /O2 /MT /EHsc /W4 /LD /DNOMINMAX /Iinclude /DFH2MOD_PLATFORM_VERSION=\"2.8.0\" ^
   src\fh2controls.cpp /Fobuild\ /Febuild\FH2Controls.dll /link /NOLOGO user32.lib gdi32.lib ole32.lib runtimeobject.lib || exit /b 1
del build\FH2Controls.exp build\FH2Controls.lib 2>nul
echo built build\FH2Controls.dll
