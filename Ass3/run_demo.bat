@echo off
setlocal enabledelayedexpansion

:MENU
cls
echo ========================================================
echo       Assignment 3 - CSMA Network Simulator
echo ========================================================
echo.
echo   1. Two-station live demo (CSMA/CD)
echo   2. Three-station collision demo (High Contention)
echo   3. Run Python simulation analysis & generate graphs
echo   4. Compile only (server.exe & client.exe)
echo   5. Exit
echo.
echo ========================================================
set /p choice="Select an option (1-5): "

if "%choice%"=="1" goto TWO_STATION
if "%choice%"=="2" goto THREE_STATION
if "%choice%"=="3" goto RUN_PYTHON
if "%choice%"=="4" goto COMPILE_ONLY
if "%choice%"=="5" goto EXIT_PROG

echo Invalid selection. Please choose 1-5.
timeout /t 2 >nul
goto MENU

:COMPILE_SUB
echo.
echo --------------------------------------------------------
echo Compiling server.cpp (with frame.cpp, socketCommon.cpp)...
g++ -O2 -std=c++11 server.cpp frame.cpp socketCommon.cpp -o server.exe -lws2_32
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Server compilation failed.
    pause
    exit /b 1
)

echo Compiling client.cpp (with frame.cpp, socketCommon.cpp)...
g++ -O2 -std=c++11 client.cpp frame.cpp socketCommon.cpp -o client.exe -lws2_32
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Client compilation failed.
    pause
    exit /b 1
)
echo [OK] Both server.exe and client.exe compiled successfully!
echo --------------------------------------------------------
exit /b 0

:TWO_STATION
call :COMPILE_SUB
if %ERRORLEVEL% NEQ 0 goto MENU

echo.
echo [1/3] Launching CSMA Shared Channel Server on Port 5050...
start "CSMA Server" cmd /k "server.exe 5050"

REM Mandatory delay to ensure server socket binds and listens before clients connect
echo Waiting 2 seconds for server initialization...
timeout /t 2 /nobreak >nul

echo [2/3] Launching Station 1 in separate window...
start "Station 1" cmd /k "client.exe 1 5050 csma-cd"

REM Slight offset so stations begin near-simultaneously but contend naturally
timeout /t 1 /nobreak >nul

echo [3/3] Launching Station 2 in separate window...
start "Station 2" cmd /k "client.exe 2 5050 csma-cd"

echo.
echo Demo started! Check the spawned terminal windows.
pause
goto MENU

:THREE_STATION
call :COMPILE_SUB
if %ERRORLEVEL% NEQ 0 goto MENU

echo.
echo [1/4] Launching CSMA Shared Channel Server on Port 5050...
start "CSMA Server" cmd /k "server.exe 5050"

REM Mandatory delay for server to begin listening
echo Waiting 2 seconds for server initialization...
timeout /t 2 /nobreak >nul

echo [2/4] Launching Station 1 (CSMA/CD)...
start "Station 1" cmd /k "client.exe 1 5050 csma-cd"

echo [3/4] Launching Station 2 (CSMA/CD)...
start "Station 2" cmd /k "client.exe 2 5050 csma-cd"

echo [4/4] Launching Station 3 (CSMA/CD)...
start "Station 3" cmd /k "client.exe 3 5050 csma-cd"

echo.
echo 3-Station collision demo running!
pause
goto MENU

:RUN_PYTHON
echo.
echo --------------------------------------------------------
echo Running statistical simulation & generating figures...
echo --------------------------------------------------------
python simulation_analysis.py
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Python simulation script encountered an error.
) else (
    echo.
    echo [SUCCESS] Figures and simulation report generated!
    echo   - p_persistent_analysis.png
    echo   - csma_comparison_analysis.png
    echo   - results.csv
    echo   - simulation_report.md
)
pause
goto MENU

:COMPILE_ONLY
call :COMPILE_SUB
pause
goto MENU

:EXIT_PROG
echo Exiting. Goodbye!
exit /b 0
