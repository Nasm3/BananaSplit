@echo off
rem Installs the Splitter virtual cables (asks for administrator rights).
rem Options are passed on to install.ps1, e.g.: install.cmd -Force
net session >nul 2>&1
if %errorlevel% neq 0 (
    rem Hand path and options over in variables, so quotes or apostrophes
    rem in them can't break the PowerShell command.
    set "SPLITTER_SELF=%~f0"
    set "SPLITTER_ARGS=%*"
    powershell -NoProfile -Command "$a = $env:SPLITTER_ARGS; if ($a) { Start-Process -FilePath $env:SPLITTER_SELF -ArgumentList $a -Verb RunAs } else { Start-Process -FilePath $env:SPLITTER_SELF -Verb RunAs }"
    exit /b
)
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0install.ps1" %*
echo.
pause
