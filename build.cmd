@echo off
setlocal
set "PATH=C:\Program Files (x86)\Dev-Cpp\MinGW64\bin;%PATH%"
cd /d "%~dp0"
windres.exe app.rc -O coff -o app.o
if errorlevel 1 exit /b 1
g++.exe -std=c++11 -O2 -Wall -Wextra -Wno-missing-field-initializers -municode -mwindows -static -static-libgcc -static-libstdc++ tray.cpp app.o -o NetSwitch.exe -lshell32 -lole32 -ladvapi32 -luuid -lcomctl32 -liphlpapi -lws2_32
if errorlevel 1 exit /b 1
g++.exe -std=c++11 -O2 -Wall -Wextra -Wno-missing-field-initializers -municode -mwindows -static -static-libgcc -static-libstdc++ service.cpp app.o -o NetSwitchService.exe -liphlpapi -lws2_32 -lole32 -loleaut32 -lwbemuuid -ladvapi32 -lshell32 -luuid
if errorlevel 1 exit /b 1
windres.exe setup.rc -O coff -o setup.o
if errorlevel 1 exit /b 1
g++.exe -std=c++11 -O2 -Wall -Wextra -Wno-missing-field-initializers -municode -mwindows -static -static-libgcc -static-libstdc++ setup.cpp setup.o -o Setup.exe -lshell32 -lole32 -loleaut32 -lwbemuuid -ladvapi32 -luuid -liphlpapi -lws2_32
exit /b %errorlevel%
