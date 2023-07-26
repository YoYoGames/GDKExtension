@echo off
:: RK - this is useful for debugging what variables are being passed in from GMS2
:: set
:: echo #########################################################################################################################

:: ensure that YYPLATFORM_option_windows_copy_exe_to_dest is set to True
if not "%YYPLATFORM_option_windows_copy_exe_to_dest%" == "True" goto error_ensure_windows_copy_exe_to_dest

:: Check if GDK is installed
if not exist "C:\Program Files (x86)\Microsoft GDK\Command Prompts\GamingDesktopVars.cmd" goto error_install_GDK

:: Setup the GDK Environment (force version Update Oct 2021)
set GRDKEDITION=230600
call "C:\Program Files (x86)\Microsoft GDK\Command Prompts\GamingDesktopVars.cmd" GamingDesktopVS2019
if ERRORLEVEL 1 (
  goto error_wrong_GDK
)

:: Ensure the runner is called the correct thing
pushd "%YYoutputFolder%"

:: Resolve ${project_name} if used
call :getfilename "%YYPLATFORM_option_windows_executable_name%"

:: Rename the runner to the executable name (GameOptions->Windows->Executable Name)
if exist Runner.exe move Runner.exe "%filename%.exe"

:: Copy the required dll libraries from the user's GDK installation folder
set GDK_PATH=%GameDK%\%GRDKEDITION%\GRDK\ExtensionLibraries
if not exist "Party.dll" copy "%GDK_PATH%\PlayFab.Party.Cpp\Redist\CommonConfiguration\neutral\Party.dll" "Party.dll"
if not exist "PartyXboxLive.dll" copy "%GDK_PATH%\PlayFab.PartyXboxLive.Cpp\Redist\CommonConfiguration\neutral\PartyXboxLive.dll" "PartyXboxLive.dll"
if not exist "XCurl.dll" copy "%GDK_PATH%\Xbox.XCurl.API\Redist\CommonConfiguration\neutral\XCurl.dll" "XCurl.dll"

:: Get path to the game (*.win) under YYC the output game isn't named correctly
FOR %%F IN (*.win) DO (
  set outputPath=%%F
  goto break
)
:break
set outputPath=%cd%\%outputPath%

popd

:: register the application
wdapp register "%YYoutputFolder%" >"%YYtempFolderUnmapped%\wdapp.out"
if ERRORLEVEL 1 (
  type "%YYtempFolderUnmapped%\wdapp.out"
  goto exitError
)

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
  
  :: Delete older output files
  copy NUL "%YYtempFolderUnmapped%\game.out"
	wdapp launch %APPNAME% -outputdebugstring -game "%outputPath%" -debugoutput %YYtempFolderUnmapped%\game.out -output %YYtempFolderUnmapped%\game.out
	powershell "Get-Content '%YYtempFolderUnmapped%\game.out' -Wait -Encoding utf8 -Tail 30 | ForEach-Object { $_; if($_ -match '###game_end###') { break } }"

	exit /b 255
)

:exit
exit /b 0

:: ----------------------------------------------------------------------------------------------------
:exitError
echo "ERROR : Unable to complete"
exit /b 1

:: ----------------------------------------------------------------------------------------------------
:: If the GDK is not installed then prompt the user to install it
:error_install_GDK
echo "Goto https://github.com/microsoft/GDK/releases/tag/October_2021_Republish to install the GDK"
exit /b 1

:: ----------------------------------------------------------------------------------------------------
:: If the required GDK verison is not installed 
:error_wrong_GDK
echo "Wrong GDK version, goto https://github.com/microsoft/GDK/releases/tag/October_2021_Republish"
exit /b 1

:: ----------------------------------------------------------------------------------------------------
:: Ensire that windows option for copy exe to dest is enabled
:error_ensure_windows_copy_exe_to_dest
echo "The Game Options -> Windows -> General -> Copy exe to output folder MUST be enabled."
exit /b 1

:: ----------------------------------------------------------------------------------------------------
:: Get the filename from the given parameter
:getfilename
:: First we remove the extension (since dev could have missed it in the IDE)
set filename=%~n1
:: Resolve ${project_name} if it was used
call set filename=%%filename:${project_name}=%YYMACROS_project_name%%%
goto :eof