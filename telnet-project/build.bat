@echo off
setlocal enabledelayedexpansion

echo ===================================================
echo   Compiling Telnet Project on Windows (C++17 Standalone)
echo ===================================================

set "CXX=C:\msys64\ucrt64\bin\g++.exe"
if not exist "%CXX%" (
    where g++ >nul 2>nul
    if !ERRORLEVEL! equ 0 (
        set "CXX=g++"
    ) else (
        echo [ERROR] No g++ compiler found. Please install MSYS2 or MinGW.
        exit /b 1
    )
)

echo Using compiler: %CXX%

if not exist build mkdir build

echo [1/6] Compiling shared library objects...
"%CXX%" -std=c++17 -Wall -Wextra -Wno-unused-parameter -Iinclude -c src/socket.cpp -o build/socket.o
if !ERRORLEVEL! neq 0 exit /b 1

"%CXX%" -std=c++17 -Wall -Wextra -Wno-unused-parameter -Iinclude -c src/nvt.cpp -o build/nvt.o
if !ERRORLEVEL! neq 0 exit /b 1

"%CXX%" -std=c++17 -Wall -Wextra -Wno-unused-parameter -Iinclude -c src/telnet_protocol.cpp -o build/telnet_protocol.o
if !ERRORLEVEL! neq 0 exit /b 1

"%CXX%" -std=c++17 -Wall -Wextra -Wno-unused-parameter -Iinclude -c src/terminal_session.cpp -o build/terminal_session.o
if !ERRORLEVEL! neq 0 exit /b 1

"%CXX%" -std=c++17 -Wall -Wextra -Wno-unused-parameter -Iinclude -c src/telnet_server.cpp -o build/telnet_server_lib.o
if !ERRORLEVEL! neq 0 exit /b 1

"%CXX%" -std=c++17 -Wall -Wextra -Wno-unused-parameter -Iinclude -c src/telnet_client.cpp -o build/telnet_client_lib.o
if !ERRORLEVEL! neq 0 exit /b 1

set LIB_OBJS=build/socket.o build/nvt.o build/telnet_protocol.o build/terminal_session.o build/telnet_server_lib.o build/telnet_client_lib.o

echo [2/6] Building standalone telnet_server.exe...
"%CXX%" -std=c++17 -static -Iinclude src/server.cpp %LIB_OBJS% -lws2_32 -o build/telnet_server.exe
if !ERRORLEVEL! neq 0 exit /b 1

echo [3/6] Building standalone telnet_client.exe...
"%CXX%" -std=c++17 -static -Iinclude src/client.cpp %LIB_OBJS% -lws2_32 -o build/telnet_client.exe
if !ERRORLEVEL! neq 0 exit /b 1

echo [4/6] Building standalone test_nvt.exe...
"%CXX%" -std=c++17 -static -Iinclude tests/test_nvt.cpp %LIB_OBJS% -lws2_32 -o build/test_nvt.exe
if !ERRORLEVEL! neq 0 exit /b 1

echo [5/6] Building standalone test_protocol.exe...
"%CXX%" -std=c++17 -static -Iinclude tests/test_protocol.cpp %LIB_OBJS% -lws2_32 -o build/test_protocol.exe
if !ERRORLEVEL! neq 0 exit /b 1

echo [6/6] Building standalone test_session.exe and integration_test.exe...
"%CXX%" -std=c++17 -static -Iinclude tests/test_session.cpp %LIB_OBJS% -lws2_32 -o build/test_session.exe
if !ERRORLEVEL! neq 0 exit /b 1

"%CXX%" -std=c++17 -static -Iinclude tests/integration_test.cpp %LIB_OBJS% -lws2_32 -o build/integration_test.exe
if !ERRORLEVEL! neq 0 exit /b 1

echo ===================================================
echo   BUILD SUCCESSFUL! (Zero external DLL dependencies)
echo ===================================================
echo Binaries created in build\:
echo   build\telnet_server.exe
echo   build\telnet_client.exe
echo   build\test_nvt.exe
echo   build\test_protocol.exe
echo   build\test_session.exe
echo   build\integration_test.exe
echo.
echo Run the server:
echo   .\build\telnet_server.exe 5000
echo.
echo Run the client:
echo   .\build\telnet_client.exe 127.0.0.1 5000
echo.
