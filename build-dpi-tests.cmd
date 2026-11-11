@echo off
setlocal
set "PATH=C:\Program Files (x86)\Dev-Cpp\MinGW64\bin;%PATH%"
cd /d "%~dp0"
windres.exe app.rc -O coff -o app.o
if errorlevel 1 exit /b 1
g++.exe -std=c++11 -O1 -Wall -Wextra -Wno-missing-field-initializers -static -static-libgcc -static-libstdc++ dpi_acceptance.cpp app.o -o dpi_acceptance.exe -liphlpapi -lws2_32 -lole32 -loleaut32 -lwbemuuid -ladvapi32 -lshell32 -luuid -lgdi32
exit /b %errorlevel%
