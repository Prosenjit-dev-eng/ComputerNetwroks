@echo off
setlocal enabledelayedexpansion

:: ============================================================================
:: Assignment 4: CDMA with Walsh Code - Windows Batch Demonstration Launcher
:: CSE/PC/B/S/314: Computer Networks Lab
:: ============================================================================

title CDMA Network Emulation (Walsh Code) Launcher
color 0B

:MENU
cls
echo ================================================================================
echo         CSE/PC/B/S/314: COMPUTER NETWORKS LAB - ASSIGNMENT 4 (CDMA)
echo ================================================================================
echo.
echo   [1] All-in-One Multi-Threaded Simulation (4 Stations, Full IEEE 802 Frames)
echo   [2] All-in-One Multi-Threaded Simulation (8 Stations, Full IEEE 802 Frames)
echo   [3] Step-by-Step Educational CDMA Math Breakdown (Inner Products & Chips)
echo   [4] Additive Channel Noise (AWGN) Stress Test (SNR = 5 dB)
echo   [5] Standalone Multi-Process Demo (Channel Server + 4 Stations in New Windows)
echo   [6] Run Python Statistical Analysis & Generate High-Res Graphs
echo   [7] Recompile All C++ Executables (MinGW GCC)
echo   [8] Exit
echo.
echo ================================================================================
set /p opt="Select an option [1-8]: "

if "%opt%"=="1" goto RUN_SIM4
if "%opt%"=="2" goto RUN_SIM8
if "%opt%"=="3" goto RUN_MATH
if "%opt%"=="4" goto RUN_NOISE
if "%opt%"=="5" goto RUN_MULTIPROC
if "%opt%"=="6" goto RUN_PY
if "%opt%"=="7" goto RECOMPILE
if "%opt%"=="8" goto EXIT

echo Invalid option. Please select 1-8.
pause
goto MENU

:RUN_SIM4
cls
echo Running All-in-One Simulation with 4 Stations...
simulation.exe 4 1
echo.
pause
goto MENU

:RUN_SIM8
cls
echo Running All-in-One Simulation with 8 Stations...
simulation.exe 8 1
echo.
pause
goto MENU

:RUN_MATH
cls
echo Running Step-by-Step Mathematical Matrix Demo...
simulation.exe 4 2
echo.
pause
goto MENU

:RUN_NOISE
cls
echo Running AWGN Channel Noise Stress Test (SNR = 5 dB)...
simulation.exe 4 3 5.0
echo.
pause
goto MENU

:RUN_MULTIPROC
cls
echo Starting Standalone Multi-Process Demo...
echo 1. Launching CDMA Channel Server (Port 5050, 4 Stations)...
start "CDMA Channel Server" cmd /k "channel_server.exe 5050 4"
timeout /t 2 >nul

echo 2. Launching 4 Independent Station Processes...
start "CDMA Station 0" cmd /k "station.exe Station_0 127.0.0.1 5050"
timeout /t 1 >nul
start "CDMA Station 1" cmd /k "station.exe Station_1 127.0.0.1 5050"
timeout /t 1 >nul
start "CDMA Station 2" cmd /k "station.exe Station_2 127.0.0.1 5050"
timeout /t 1 >nul
start "CDMA Station 3" cmd /k "station.exe Station_3 127.0.0.1 5050"
echo.
echo All processes launched in separate windows!
pause
goto MENU

:RUN_PY
cls
echo Executing Python Simulation Analysis & Generating Graphs...
python simulation_analysis.py
echo.
echo Plots generated:
echo   - cdma_orthogonality_matrix.png
echo   - cdma_signal_waveforms.png
echo   - cdma_ber_vs_snr.png
echo   - cdma_throughput_scaling.png
echo CSV exported:
echo   - results.csv
echo.
pause
goto MENU

:RECOMPILE
cls
echo Compiling C++ Source Files with g++...
g++ -O2 -std=c++11 channel_server.cpp cdma.cpp frame.cpp socketCommon.cpp -o channel_server.exe -lws2_32
g++ -O2 -std=c++11 station.cpp cdma.cpp frame.cpp socketCommon.cpp -o station.exe -lws2_32
g++ -O2 -std=c++11 simulation.cpp cdma.cpp frame.cpp socketCommon.cpp -o simulation.exe -lws2_32
echo Compilation finished!
pause
goto MENU

:EXIT
echo Exiting launcher.
