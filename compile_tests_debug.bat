@call cl.exe ^
src/tests.cpp ^
obj_debug/stdafx.obj ^
/Yustdafx.hpp ^
/D_CRT_SECURE_NO_WARNINGS ^
/std:c++20 ^
/nologo ^
/W4 ^
/EHsc ^
/Fo:obj_debug\ ^
/Fd:dist/vc140_debug.pdb ^
/Fe:dist/swan_tests_debug.exe ^
/I"C:/code/glfw" ^
/I"C:/code/boost_1_80_0" ^
/MT ^
/Zi ^
/link ^
/NATVIS:swan.natvis ^
/DEBUG:FULL ^
kernel32.lib ^
user32.lib ^
gdi32.lib ^
shell32.lib ^
msvcrt.lib ^
libcmt.lib ^
msvcrt.lib ^
shlwapi.lib
