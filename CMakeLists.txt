cmake_minimum_required(VERSION 3.22)
project(swan CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_VERBOSE_MAKEFILE OFF)

add_compile_options(/EHsc /MP /MT) # common compile options
add_link_options(/NODEFAULTLIB:MSVCRTD)

add_library(imgui STATIC
    src/imgui/imgui.cpp
    src/imgui/imgui_demo.cpp
    src/imgui/imgui_draw.cpp
    src/imgui/imgui_impl_glfw.cpp
    src/imgui/imgui_impl_opengl3.cpp
    src/imgui/imgui_tables.cpp
    src/imgui/imgui_widgets.cpp
)
set_property(TARGET imgui PROPERTY
    MSVC_RUNTIME_LIBRARY "MultiThreaded" # to eliminate MSVC warning D9025 `overriding '/MDd' with '/MT'`
)
target_include_directories(imgui PRIVATE
    "C:/code/glfw"
)
target_link_directories(imgui PRIVATE
    "C:/code/glfw"
)
target_link_options(imgui PRIVATE
    /Z7 # embed debug info directly into .obj files, to enable stepping into imgui code during debug
)

add_library(stb_image STATIC
    src/libs/stb_image.cpp
)
set_property(TARGET stb_image PROPERTY
    MSVC_RUNTIME_LIBRARY "MultiThreaded" # to eliminate MSVC warning D9025 `overriding '/MDd' with '/MT'`
)

add_executable(swan_debug
    src/libs/ntest.cpp
    src/bulk_rename_logic.cpp
    src/debug_log.cpp
    src/explorer.cpp
    src/explorer_file_op_progress_sink.cpp
    src/file_operations.cpp
    src/finder.cpp
    src/focused_window.cpp
    src/icon_glyphs.cpp
    src/icon_library.cpp
    src/imgui_dependent_functions.cpp
    src/imgui_extension.cpp
    src/miscellaneous_functions.cpp
    src/miscellaneous_globals.cpp
    src/path.cpp
    src/pinned.cpp
    src/popup_modal_bulk_rename.cpp
    src/popup_modal_edit_pin.cpp
    src/popup_modal_error.cpp
    src/popup_modal_new_directory.cpp
    src/popup_modal_new_file.cpp
    src/popup_modal_new_pin.cpp
    src/popup_modal_single_rename.cpp
    src/popup_modal_open_state_tracker.cpp
    src/recent_files.cpp
    src/settings.cpp
    src/stdafx.cpp
    src/style.cpp
    src/swan.cpp
    src/tests.cpp
    src/theme_editor.cpp
    src/undelete_directory_progress_sink.cpp
    src/util.cpp
)
set_property(TARGET swan_debug PROPERTY
    MSVC_RUNTIME_LIBRARY "MultiThreaded" # to eliminate MSVC warning D9025 `overriding '/MDd' with '/MT'`
)
target_compile_options(swan_debug PRIVATE
    /W4
    /D_CRT_SECURE_NO_WARNINGS
)
target_include_directories(swan_debug PRIVATE
    ${ENV_INCLUDE}
    "C:/code/boost_1_80_0"
    "C:/code/glfw"
    "C:/Program Files (x86)/Microsoft Visual Studio/2019/Community/VC/Tools/MSVC/14.29.30133/include"
    "C:/Program Files (x86)/Windows Kits/10/Include/10.0.19041.0/cppwinrt"
    "C:/Program Files (x86)/Windows Kits/10/Include/10.0.19041.0/shared"
    "C:/Program Files (x86)/Windows Kits/10/Include/10.0.19041.0/ucrt"
    "C:/Program Files (x86)/Windows Kits/10/Include/10.0.19041.0/um"
    "C:/Program Files (x86)/Windows Kits/10/Include/10.0.19041.0/winrt"
)
target_link_directories(swan_debug PRIVATE
    ${ENV_LIB}
    "C:/code/boost_1_80_0/stage/lib"
    "C:/code/glfw"
)
target_link_libraries(swan_debug PRIVATE
    imgui
    stb_image
    "opengl32.lib"
    "glfw3.lib"
    "gdi32.lib"
    "shell32.lib"
    "kernel32.lib"
    "msvcrt.lib"
    "ole32.lib"
    "shlwapi.lib"
    "Pathcch.lib"
    "Dbghelp.lib"
)
target_precompile_headers(swan_debug PRIVATE
    src/stdafx.hpp
)

# if (CMAKE_BUILD_TYPE STREQUAL "Debug")
# endif()
