@echo off
:: RK - this is useful for debugging what variables are being passed in from GMS2
:: set
:: echo #########################################################################################################################

:: ensure that YYPLATFORM_option_windows_copy_exe_to_dest is set to True
if not "%YYPLATFORM_option_windows_copy_exe_to_dest%" == "True"goto error_ensure_windows_copy_exe_to_dest

:: Setup the GDK Environment
if not exist "C:\Program Files (x86)\Microsoft GDK\Command Prompts\GamingDesktopVars.cmd" goto error_install_GDK
call "C:\Program Files (x86)\Microsoft GDK\Command Prompts\GamingDesktopVars.cmd" GamingDesktopVS2019


:: ensure the runner is called the correct thing
pushd %YYoutputFolder%
call :getfilename %YYcompile_output_file_name%
if exist Runner.exe move Runner.exe %filename%.exe
popd

:: register the application
wdapp register %YYoutputFolder% >"%YYtempFolderUnmapped%\wdapp.out"
if ERRORLEVEL 1 goto exit

:: can be useful for debugging problems
:: type "%YYtempFolderUnmapped%\wdapp.out"

:: get the application name, this is horrible but should find the game appname to use for launching
pushd "%YYtempFolderUnmapped%"
for /f "tokens=*" %%a in (wdapp.out) do (
  (echo %%a | findstr /i /c:"!Game" >nul) && (set APPNAME=%%a) 
)
popd

:: launch the application
if not "%APPNAME%" == "" (
	wdapp launch %APPNAME% -outputdebugstring -game %YYcompile_output_file_name% -debugoutput %YYtempFolderUnmapped%\game.out -output %YYtempFolderUnmapped%\game.out
	powershell Get-Content "%YYtempFolderUnmapped%\game.out" -Wait -Encoding utf8 -Tail 30
	exit /b 255
)

:exit
exit /b 0

:: ----------------------------------------------------------------------------------------------------
:: If the GDK is not installed then prompt the user to install it
:error_install_GDK
echo Goto https://github.com/microsoft/GDK to install the GDK
exit /b 1

:: ----------------------------------------------------------------------------------------------------
:: Ensire that windows option for copy exe to dest is enabled
:error_ensure_windows_copy_exe_to_dest
echo The Game Options -> Windows -> General -> Copy exe to output folder MUST be enabled.
exit /b 1

:: ----------------------------------------------------------------------------------------------------
:: Get the filename from the given parameter
:getfilename
set filename=%~n1
goto :eof