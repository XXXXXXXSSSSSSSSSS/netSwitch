@echo off
setlocal
set "PATH=C:\Program Files (x86)\Dev-Cpp\MinGW64\bin;%PATH%"
cd /d "%~dp0"
g++.exe -std=c++11 -O1 -Wall -Wextra -Wno-missing-field-initializers -static -static-libgcc -static-libstdc++ acceptance.cpp -o acceptance.exe -liphlpapi -lws2_32 -lole32 -loleaut32 -lwbemuuid -ladvapi32 -lshell32 -luuid
exit /b %errorlevel%
