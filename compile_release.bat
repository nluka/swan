@call cl.exe ^
src/all_in_one.cpp ^
obj_release/imgui_demo.obj ^
obj_release/imgui_draw.obj ^
obj_release/imgui_impl_glfw.obj ^
obj_release/imgui_impl_opengl3.obj ^
obj_release/imgui_tables.obj ^
obj_release/imgui_widgets.obj ^
obj_release/imgui.obj ^
obj_release/stb_image.obj ^
resource/swan.res ^
/Fo:obj_release\ ^
/Fe:dist/swan_release.exe ^
/Fd:dist/vc140_release.pdb ^
/I"C:/code/glfw" ^
/I"C:/code/boost_1_80_0" ^
/std:c++20 ^
/nologo ^
/W4 ^
/EHsc ^
/O2 ^
/MP ^
/MT ^
/D_CRT_SECURE_NO_WARNINGS ^
/DNDEBUG ^
/Zi ^
/link ^
/NODEFAULTLIB:MSVCRTD ^
/NODEFAULTLIB:LIBCMT ^
/LIBPATH:"C:/code/glfw" ^
/LIBPATH:"C:/code/boost_1_80_0/stage/lib" ^
glfw3.lib ^
opengl32.lib ^
gdi32.lib ^
shell32.lib ^
kernel32.lib ^
msvcrt.lib ^
ole32.lib ^
shlwapi.lib ^
Pathcch.lib ^
Dbghelp.lib
