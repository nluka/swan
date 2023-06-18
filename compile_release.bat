@REM @echo off

@REM @echo Compiling nexplorer release...

@call cl.exe /O2 /DNDEBUG /std:c++20 /nologo /W4 /WX /EHsc /Fo:obj_release\ /Fe:nexplorer_release.exe /I"C:/code/glfw" /I"C:/code/boost_1_80_0" /MT ^
src/main.cpp src/util.cpp ^
obj_release/imgui_demo.obj ^
obj_release/imgui_draw.obj ^
obj_release/imgui_impl_glfw.obj ^
obj_release/imgui_impl_opengl3.obj ^
obj_release/imgui_tables.obj ^
obj_release/imgui_widgets.obj ^
obj_release/imgui.obj ^
/link /NODEFAULTLIB:MSVCRTD /NODEFAULTLIB:LIBCMT ^
/LIBPATH:"C:/code/glfw" /LIBPATH:"C:/code/boost_1_80_0/stage/lib" ^
glfw3.lib opengl32.lib kernel32.lib user32.lib gdi32.lib shell32.lib msvcrt.lib libcmt.lib ^
odbccp32.lib odbc32.lib uuid.lib oleaut32.lib ole32.lib advapi32.lib comdlg32.lib winspool.lib msvcrt.lib

@REM @if %errorlevel% equ 0 (
@REM     @call nexplorer_debug.exe
@REM )
