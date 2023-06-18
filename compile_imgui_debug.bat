@echo Compiling imgui debug...

@call cl.exe /std:c++20 /W4 /WX /nologo /EHsc /Fo:obj_debug\ /c /I"C:/code/glfw" ^
src/imgui/imgui.cpp ^
src/imgui/imgui_demo.cpp ^
src/imgui/imgui_draw.cpp ^
src/imgui/imgui_tables.cpp ^
src/imgui/imgui_widgets.cpp ^
src/imgui/imgui_impl_glfw.cpp ^
src/imgui/imgui_impl_opengl3.cpp
