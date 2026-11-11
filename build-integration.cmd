@echo off
setlocal
set "PATH=C:\Program Files (x86)\Dev-Cpp\MinGW64\bin;%PATH%"
cd /d "%~dp0"
g++.exe -std=c++11 -O2 -static -static-libgcc -static-libstdc++ integration.cpp -o integration.exe -lshell32 -lole32 -ladvapi32 -luuid
exit /b %errorlevel%
