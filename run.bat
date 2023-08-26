make
@IF %ERRORLEVEL% NEQ 0 (echo "Make returned an error: %ERRORLEVEL%" & exit /B)
lightware.exe
@IF %ERRORLEVEL% NEQ 0 (echo "program returned an error: %ERRORLEVEL%" & exit /B)