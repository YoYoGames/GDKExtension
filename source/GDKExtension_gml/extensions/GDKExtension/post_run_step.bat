@echo off
set Utils="%~dp0scriptUtils.bat"

:: ######################################################################################
:: Script Logic

:: Always init the script
call %Utils% scriptInit

:: ensure that YYPLATFORM_option_windows_copy_exe_to_dest is set to True
if not "%YYPLATFORM_option_windows_copy_exe_to_dest%" == "True" (
	call %Utils% logError "The Game Options -> Windows -> General -> Copy exe to output folder MUST be enabled."
)

:: Check if GDK is installed
if not exist "C:\Program Files (x86)\Microsoft GDK\Command Prompts\GamingDesktopVars.cmd" (
	call %Utils% logError "Goto https://github.com/microsoft/GDK/releases/tag/June_2023_Update_2 to install the GDK"
)

:: Check if GDK version is correct
call %Utils% optionGetValue "gdkVersion" GDK_VERSION
set GDK_PATH=%GameDK%%GDK_VERSION%\GRDK\ExtensionLibraries
if not exist "%GDK_PATH%" (
	call %Utils% logError "Wrong GDK version, goto https://github.com/microsoft/GDK/releases/tag/June_2023_Update_2"
)

:: Setup the GDK Environment (force version Update Jun 2023)
call "C:\Program Files (x86)\Microsoft GDK\Command Prompts\GamingDesktopVars.cmd" GamingDesktopVS2022
if ERRORLEVEL 1 (
	call %Utils% logError "Failed to setup GDK Environment (using GamingDesktopVS2022)"
)

:: Ensure the runner is called the correct thing
pushd "%YYoutputFolder%"

:: Resolve ${project_name} if used
call :expandFilename "%YYPLATFORM_option_windows_executable_name%" FILENAME

if not defined FILENAME (
	call %Utils% logError "Unable to expand the executable filename"
)

:: Rename the runner to the executable name (GameOptions->Windows->Executable Name)
if exist Runner.exe move Runner.exe "%FILENAME%.exe" >nul 2>&1

:: Copy the required dll libraries from the user's GDK installation folder
call %Utils% itemCopyTo "%GDK_PATH%\PlayFab.Party.Cpp\Redist\CommonConfiguration\neutral\Party.dll" "Party.dll"
call %Utils% itemCopyTo "%GDK_PATH%\PlayFab.PartyXboxLive.Cpp\Redist\CommonConfiguration\neutral\PartyXboxLive.dll" "PartyXboxLive.dll"
call %Utils% itemCopyTo "%GDK_PATH%\Xbox.XCurl.API\Redist\CommonConfiguration\neutral\XCurl.dll" "XCurl.dll"

:: Get path to the game (*.win) under YYC the output game isn't named correctly
for %%f in (*.win) do (
	set outputPath=%%f
	goto extractPathdone
)
:extractPathdone
set outputPath=%cd%\%outputPath%

popd

:: Generate localisation data (if appropriate)
if exist "%YYoutputFolder%\GDKExtensionStrings" (
	makepkg localize /d "%YYoutputFolder%" /resw GDKExtensionStrings
	if ERRORLEVEL 1 (
		call %Utils% logError "Unable to complete 'makepkg localize'."
	)
)

:: Register the application (capture output)
set "tempOut=%YYtempFolderUnmapped%\wdapp.out"
wdapp register "%YYoutputFolder%" > "%tempOut%"
if ERRORLEVEL 1 (
	:: Print the 'wdapp register' output
	type %tempOut%
	call %Utils% logError "Unable to complete 'wdapp register'."
)

:: Use %%a directly after 'tokens=2' to capture the PFN correctly (leave on match)
for /f "tokens=2*" %%a in ('type "%tempOut%" ^| findstr /C:"Registered "') do (
    set "PFN=%%a"
    goto extractPFNdone
)

:: Check if PFN was extracted
:extractPFNdone
if not defined PFN (
	call %Utils% logError "Unable to find the Package Family Name (PFN)"
)

:: Prepare to read the wdapp list
set "tempList=%YYtempFolderUnmapped%\wdapp.list"
call %Utils% itemDelete "%tempList%"
wdapp list > "%tempList%"

:: Switching to EnableDelayedExpansion to properly handle the 'found' flag
setlocal enabledelayedexpansion

:: Initialize control variable
set found=0

:: Try to extract appname from registered app list
for /f "delims=" %%a in ('type "%tempList%"') do (
    if "!found!"=="1" (
		:: End local here so the extracted value keeps the "!" character
		endlocal
		:: Trim the matches line
		for /f "tokens=*" %%i in ("%%a") do set "APPNAME=%%i"
        goto extractAPPNAMEdone
    )
	:: Check if we found the Package Family Name
    if "%%a"=="!PFN!" set found=1
)

:extractAPPNAMEdone
if not defined APPNAME (
	call %Utils% logError "Unable to find the App Name (AUMID)"
)

:: Cleanup
call %Utils% itemDelete "%tempList%"

:: launch the application
if not "%APPNAME%" == "" (
  :: Delete older output files
  type nul > "%YYtempFolderUnmapped%\game.out"
	wdapp launch %APPNAME% -outputdebugstring -game "%outputPath%" -debugoutput %YYtempFolderUnmapped%\game.out -output %YYtempFolderUnmapped%\game.out
	powershell "Get-Content '%YYtempFolderUnmapped%\game.out' -Wait -Encoding utf8 -Tail 30 | ForEach-Object { $_; if($_ -match '###game_end###') { break } }"

	exit 255
)

call %Utils% logError "Unknown error found!"

:: ----------------------------------------------------------------------------------------------------
:: Get the filename from the given parameter
:expandFilename
	:: Fetch the value of the input filename variable
	set "input=%~n1"

	:: Need to enabled delayed expansion
    setlocal enabledelayedexpansion

	:: Replace ${project_name} with the actual project name (requires delayed expansion)
	set "output=!input:${project_name}=%YYMACROS_project_name%!"

	:: Update the caller's variable to hold the output filename
	endlocal & set "%~2=%output%"
exit /b 0