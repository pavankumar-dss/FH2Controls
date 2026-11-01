@echo off
rem Builds and runs the harness against build\FH2Controls.dll, in a scratch folder standing in for
rem Azimuth's, with the real game exe mapped (not run) so the hooks' bytes are checked against it.
setlocal
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>nul || exit /b 1
cd /d "%~dp0.."
if not exist build\test mkdir build\test
cl /nologo /std:c++17 /O2 /MT /EHsc /W4 /DNOMINMAX /Iinclude tests\harness.cpp /Fobuild\test\ /Febuild\test\harness.exe /link /NOLOGO user32.lib gdi32.lib ole32.lib runtimeobject.lib || exit /b 1
if exist build\test\scratch rmdir /s /q build\test\scratch
mkdir build\test\scratch
build\test\harness.exe build\FH2Controls.dll build\test\scratch "C:\Program Files (x86)\Steam\steamapps\common\Forza Horizon 2\GameFiles\Anthem_1.0.0.64_x64__8wekyb3d8bbwe\Mount\forza_durango_release_final.exe"
