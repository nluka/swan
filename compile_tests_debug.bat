@REM @echo off

@REM @echo Compiling swan_debug...

@call cl.exe /D_CRT_SECURE_NO_WARNINGS /std:c++20 /nologo /W4 /WX /EHsc /Fo:obj_debug\ /Fe:swan_tests_debug.exe /MT ^
src/tests.cpp ^
kernel32.lib user32.lib gdi32.lib shell32.lib msvcrt.lib libcmt.lib msvcrt.lib shlwapi.lib

@REM @if %errorlevel% equ 0 (
@REM     @call swan_debug.exe
@REM )
