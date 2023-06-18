@echo Compiling imgui release...

@call cl.exe /O2 /DNDEBUG /std:c++20 /W4 /WX /nologo /DNDEBUG /EHsc /Fo:obj_release\ /c /I"C:/code/glfw" ^
src/imgui/imgui.cpp ^
src/imgui/imgui_demo.cpp ^
src/imgui/imgui_draw.cpp ^
src/imgui/imgui_tables.cpp ^
src/imgui/imgui_widgets.cpp ^
src/imgui/imgui_impl_glfw.cpp ^
src/imgui/imgui_impl_opengl3.cpp
