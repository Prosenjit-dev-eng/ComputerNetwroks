@echo off
echo ========================================================
echo       Regenerating CSMA_REPORT.pdf from Markdown...
echo ========================================================
python build_pdf.py
if %ERRORLEVEL% EQU 0 (
    echo.
    echo [SUCCESS] PDF successfully regenerated: CSMA_REPORT.pdf
) else (
    echo.
    echo [ERROR] Failed to generate PDF.
)
pause
