/*
    Bunch of headers to precompile - things that aren't touched when developing swan: STL, STB libs, Boost, ImGui itself, etc.
*/

#include <algorithm>
#include <array>
#include <atomic>
#include <boost/archive/xml_iarchive.hpp>
#include <boost/archive/xml_oarchive.hpp>
#include <boost/circular_buffer.hpp>
#include <boost/container/static_vector.hpp>
#include <boost/stacktrace.hpp>
#include <boost/static_string.hpp>
#include <cassert>
#include <chrono>
#include <comdef.h>
#include <cstring>
#include <dbghelp.h>
#include <execution>
#include <fileapi.h>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <list>
#include <mutex>
#include <numeric>
#include <oleidl.h>
#include <pathcch.h>
#include <ranges>
#include <regex>
#include <set>
#include <shlobj_core.h>
#include <shlwapi.h>
#include <shobjidl_core.h>
#include <source_location>
#include <sstream>
#include <span>
#include <string_view>
#include <string>
#include <stringapiset.h>
#include <tchar.h>
#include <vector>
#include <windows.h>

#undef min
#undef max

#define IMGUI_DEFINE_MATH_OPERATORS 1
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"
#include "imgui/font_awesome.h"
#include "imgui/material_design.h"
#include "imgui/codicons.h"

#define GL_SILENCE_DEPRECATION
#include <glfw3.h> // Will drag system OpenGL headers

#pragma warning(push)
#pragma warning(disable: 4244)
// #pragma warning(disable: 4459)
#include "libs/stb_image.h"
#pragma warning(pop)

namespace imgui = ImGui;

#include "primitives.hpp"

#include "libs/ntest.hpp"
#include "libs/on_scope_exit_2.hpp"
#include "libs/thread_pool.hpp"
