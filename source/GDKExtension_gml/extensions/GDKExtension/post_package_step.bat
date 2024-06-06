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
popd

:: Generate localisation data (if appropriate)
if exist "%YYoutputFolder%\GDKExtensionStrings" (
	makepkg localize /d "%YYoutputFolder%" /resw GDKExtensionStrings
	if ERRORLEVEL 1 (
		call %Utils% logError "Unable to complete 'makepkg localize'."
	)
)

:: Generate map
makepkg genmap /f "%YYoutputFolder%\layout.xml" /d "%YYoutputFolder%"
if ERRORLEVEL 1 (
	call %Utils% logError "Unable to complete 'makepkg genmap'."
)

:: Create __temp named target folder (delete existing)
for %%a in ("%YYtargetFile%") do set "YYtargetFolder=%%~dpa"
set "YYtargetTempFolder=%YYtargetFolder%__temp"
mkdir "%YYtargetTempFolder%"

:: Generate package inside __temp folder
set "tempOut=%YYtempFolderUnmapped%\makepkg.out"
makepkg pack /f "%YYoutputFolder%\layout.xml" /d "%YYoutputFolder%" /pd "%YYtargetTempFolder%" -pc > "%tempOut%"
if ERRORLEVEL 1 (
	:: Print the 'makepkg pack' output
	type %tempOut%
	call %Utils% logError "Unable to complete 'makepkg pack'."
)

:: Get the name for the output package folder
for /f "tokens=2 delims='" %%a in ('type "%tempOut%" ^| findstr /C:"Successfully created package"') do (
    set "APPFOLDER=%%~na-pkg"
    goto extractAPPFOLDERdone
)

:extractAPPFOLDERdone
if not defined APPFOLDER (
	call %Utils% logError "Unable to find the output folder for the package"
)

:: Rename the output folder (delete existing)
set "YYpackageOutputFolder=%YYtargetFolder%%APPFOLDER%"
call %Utils% itemDelete "%YYpackageOutputFolder%"

ren "%YYtargetTempFolder%" "%APPFOLDER%"
if ERRORLEVEL 1 (
	call %Utils% logError "Failed to rename the output package folder"
)

:: Everything finished OKAY (send an error exit code to igor to block the build)
echo.
echo "################################### Finished Creating Package ###################################"
echo "Output folder: %YYpackageOutputFolder%"
echo "NOTE: You will need both .MSIXVC and .EKB to upload the package to the MS Partner Center"
echo "#################################################################################################"
exit 255

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
