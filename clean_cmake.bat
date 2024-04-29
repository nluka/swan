@Echo off
rmdir /s /q dist\CMakeFiles
rmdir /s /q dist\.vs
del dist\cmake_install.cmake
del dist\swan_debug.exp
del dist\swan_release.exp
del dist\ALL_BUILD.vcxproj.filters
del dist\imgui.vcxproj.filters
del dist\swan_debug.vcxproj.filters
del dist\ZERO_CHECK.vcxproj.filters
del dist\compile_commands.json
del dist\imgui.lib
del dist\stb_image.lib
del dist\swan_debug.lib
del dist\swan_release.lib
del dist\build.ninja
del dist\.ninja_deps
del dist\.ninja_log
del dist\swan_debug.pdb
del dist\swan_release.pdb
del dist\vc140_debug.pdb
del dist\vc140_release.pdb
del dist\swan.sln
del dist\CMakeCache.txt
del dist\ALL_BUILD.vcxproj
del dist\imgui.vcxproj
del dist\swan_debug.vcxproj
del dist\ZERO_CHECK.vcxproj
