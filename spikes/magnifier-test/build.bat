@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
cd /d "%~dp0"
cl /nologo /std:c++17 /O2 /EHsc /W4 /DUNICODE /D_UNICODE magtest.cpp /link /SUBSYSTEM:WINDOWS
