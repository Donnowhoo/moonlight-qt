@echo off
setlocal
rem Run from the repository root. Only builds a dependency-free policy test, never Moonlight.
call scripts\find-vswhere.bat
if errorlevel 1 exit /b 1
for /f "usebackq delims=" %%i in (`%VSWHERE% -latest -property installationPath`) do (
    call "%%i\VC\Auxiliary\Build\vcvarsall.bat" x64
)
if errorlevel 1 exit /b 1
if not exist build\touch-policy-tests mkdir build\touch-policy-tests
cl /nologo /std:c++17 /EHsc /W4 /WX /utf-8 /UNDEBUG tests\desktop_touch_policy_test.cpp /Fobuild\touch-policy-tests\policy.obj /Febuild\touch-policy-tests\policy.exe
if errorlevel 1 exit /b 1
build\touch-policy-tests\policy.exe
if errorlevel 1 exit /b 1
python tests\test_desktop_touch_guards.py
exit /b %errorlevel%
