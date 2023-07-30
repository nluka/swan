@call cl.exe ^
src/tests.cpp ^
/D_CRT_SECURE_NO_WARNINGS ^
/std:c++20 ^
/nologo ^
/W4 ^
/EHsc ^
/Fo:obj_debug\ ^
/Fe:bin\swan_tests_debug.exe ^
/MT ^
/link ^
kernel32.lib ^
user32.lib ^
gdi32.lib ^
shell32.lib ^
msvcrt.lib ^
libcmt.lib ^
msvcrt.lib ^
shlwapi.lib
