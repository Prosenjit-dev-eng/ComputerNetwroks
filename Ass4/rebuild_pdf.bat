@echo off
echo ========================================================
echo       Regenerating CDMA_REPORT.pdf from Markdown...
echo ========================================================
python build_pdf.py
if %ERRORLEVEL% EQU 0 (
    echo.
    echo [SUCCESS] PDF successfully regenerated: CDMA_REPORT.pdf and CN-4.pdf
) else (
    echo.
    echo [ERROR] Failed to generate PDF.
)
pause
