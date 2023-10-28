@echo off
call env-vc64.bat

if not exist build mkdir build

pushd build
cl -Od -Z7 -FC ..\test-bed.c
popd

if %ERRORLEVEL% geq 1 (
	echo Done with errors.
	exit /B 2
) else (
	echo Done.
)