@echo off

REM If the user provided "/?" as the commandline parameter, print the usage information
if [%~1]==[/?] goto :help 

:help
echo "Generates a visual studio solution/project for KenshiCalc."
echo ""
echo "Setup [version]"
echo ""
echo "version must be one of the premake actions. for a full list of available actions see: https://premake.github.io/docs/Using-Premake/"
echo ""
echo "if no version is provided, the default will be vs2019."
goto :EOF

REM Check if a command line argument was provided
if [%~1]==[] goto :blank else goto :cmdline-version

REM If the user passed a command line argument to the script, we set the vs-version to use to that argument
:cmdline-version
set vs-version = %~1
goto :run-premake

REM If no command line argument is provided, we default to vs2019
:blank
set vs-version = vs2019
goto :run-premake

REM Once we've set the variable depending on whether the user supplied a command line argument or not, run the premake executable
:run-premake
pushd ..
Walnut\vendor\bin\premake5.exe %vs-version%
popd
pause