@echo off
setlocal
set "PATH=C:\Program Files (x86)\Dev-Cpp\MinGW64\bin;%PATH%"
cd /d "%~dp0"
g++.exe -std=c++11 -O2 -Wall -Wextra -Wno-missing-field-initializers -static -static-libgcc -static-libstdc++ smoke.cpp -o smoke.exe -liphlpapi -lws2_32 -lole32 -ladvapi32 -lshell32 -luuid -lpsapi
exit /b %errorlevel%
