cmake_minimum_required(VERSION 3.22)
project(swan CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_VERBOSE_MAKEFILE OFF)

option(CMAKE_BUILD_TYPE "Build type, Debug or Release")
option(GLFW3_PATH "Path to directory containing <glfw3.h> and <glfw3.lib> files")
option(GLEW_PATH "Path to directory containing <glew.h> and <glew32s.lib> files")
option(BOOST_PATH "Path to standard Boost 1.80.0 directory")

if(NOT GLFW3_PATH)
    message(FATAL_ERROR "Please provide path to directory containing <glfw3.h> and <glfw3.lib> files with -DGLFW3_PATH=/path/to/dir")
endif()
if(NOT GLEW_PATH)
    message(FATAL_ERROR "Please provide path to directory containing <glew.h> and <glew.lib> files with -DGLEW_PATH=/path/to/dir")
endif()
if(NOT BOOST_PATH)
    message(FATAL_ERROR "Please provide path to Boost 1.80.0 directory with -DBOOST_PATH=/path/to/dir")
endif()
if(NOT CMAKE_BUILD_TYPE)
    message(FATAL_ERROR "Please specify build type as Debug or Release")
endif()

add_compile_options(/EHsc /MP /MT)
add_link_options(/NODEFAULTLIB:MSVCRTD)
add_link_options(/NODEFAULTLIB:LIBCMT)
add_link_options(/SUBSYSTEM:WINDOWS)

add_library(imgui STATIC
    src/imgui/imgui.cpp
    src/imgui/imgui_demo.cpp
    src/imgui/imgui_draw.cpp
    src/imgui/imgui_impl_glfw.cpp
    src/imgui/imgui_impl_opengl3.cpp
    src/imgui/imgui_impl_win32.cpp
    src/imgui/imgui_impl_dx11.cpp
    src/imgui/imgui_tables.cpp
    src/imgui/imgui_widgets.cpp
    src/imgui/imgui_stdlib.cpp
)
set_property(TARGET imgui PROPERTY
    MSVC_RUNTIME_LIBRARY "MultiThreaded" # to eliminate MSVC warning D9025 `overriding '/MDd' with '/MT'`
)
target_include_directories(imgui PRIVATE
    ${GLFW3_INCLUDE_PATH}
)
target_link_directories(imgui PRIVATE
    ${GLFW3_LIB_PATH}
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

set(SWAN_SOURCES
    "src/libs/ntest.cpp"
    "src/analytics.cpp"
    "src/debug_log.cpp"
    "src/explorer_drop_source.cpp"
    "src/explorer_file_op_progress_sink.cpp"
    "src/explorer.cpp"
    "src/file_operations.cpp"
    "src/finder.cpp"
    "src/focused_window.cpp"
    "src/icon_glyphs.cpp"
    "src/icon_library.cpp"
    "src/imgui_dependent_functions.cpp"
    "src/imgui_extension.cpp"
    "src/main_menu_bar.cpp"
    "src/miscellaneous_functions.cpp"
    "src/miscellaneous_globals.cpp"
    "src/path.cpp"
    "src/pinned.cpp"
    "src/popup_modal_bulk_rename.cpp"
    "src/popup_modal_edit_pin.cpp"
    "src/popup_modal_error.cpp"
    "src/popup_modal_new_directory.cpp"
    "src/popup_modal_new_file.cpp"
    "src/popup_modal_new_pin.cpp"
    "src/popup_modal_single_rename.cpp"
    "src/recent_files.cpp"
    "src/settings.cpp"
    "src/stdafx.cpp"
    "src/style.cpp"
    # "src/swan_win32_dx11.cpp"
    "src/swan_glfw_opengl3.cpp"
    "src/tests.cpp"
    "src/theme_editor.cpp"
    "src/undelete_directory_progress_sink.cpp"
    "src/util.cpp"
)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_executable(swan_debug
        ${SWAN_SOURCES}
    )
    set_property(TARGET swan_debug PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded" # to eliminate MSVC warning D9025 `overriding '/MDd' with '/MT'`
    )
    target_compile_options(swan_debug PRIVATE
        /W4
        /D_CRT_SECURE_NO_WARNINGS
    )
    target_include_directories(swan_debug PRIVATE
        ${BOOST_PATH}
        ${GLFW3_PATH}
        ${GLEW_PATH}
    )
    target_link_directories(swan_debug PRIVATE
        ${BOOST_LIB_PATH}/stage/lib
        ${GLEW_PATH}
    )
    target_link_libraries(swan_debug PRIVATE
        imgui
        stb_image
        "opengl32.lib"
        "glfw3.lib"
        "glew32s.lib"
        "D3D11.lib"
        "gdi32.lib"
        "shell32.lib"
        "kernel32.lib"
        "msvcrt.lib"
        "ole32.lib"
        "shlwapi.lib"
        "Pathcch.lib"
        "Dbghelp.lib"
    )
    target_link_options(swan_debug PRIVATE
        /NATVIS:${CMAKE_CURRENT_LIST_DIR}/swan.natvis
    )
    target_precompile_headers(swan_debug PRIVATE
        src/stdafx.hpp
    )

elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_executable(swan_release
        ${SWAN_SOURCES}
        "resource/swan.rc"
    )
    set_property(TARGET swan_release PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded" # to eliminate MSVC warning D9025 `overriding '/MDd' with '/MT'`
    )
    target_compile_options(swan_release PRIVATE
        /W4
        /D_CRT_SECURE_NO_WARNINGS
        /DNDEBUG
        /O2
    )
    target_include_directories(swan_release PRIVATE
        ${BOOST_PATH}
        ${GLFW3_INCLUDE_PATH}
    )
    target_link_directories(swan_release PRIVATE
        ${BOOST_LIB_PATH}
        ${GLFW3_LIB_PATH}
    )
    target_link_libraries(swan_release PRIVATE
        imgui
        stb_image
        "opengl32.lib"
        "glfw3.lib"
        "glew32s.lib"
        "gdi32.lib"
        "shell32.lib"
        "kernel32.lib"
        "msvcrt.lib"
        "ole32.lib"
        "shlwapi.lib"
        "Pathcch.lib"
        "Dbghelp.lib"
    )
    target_precompile_headers(swan_release PRIVATE
        src/stdafx.hpp
    )

else()
    message(FATAL_ERROR "Incorrect build type, specify Debug or Release")
endif()
