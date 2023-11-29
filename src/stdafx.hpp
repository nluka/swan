#include "primitives.hpp"
#include "libs/thread_pool.hpp"
#include "libs/on_scope_exit.hpp"
#include "libs/on_scope_exit_2.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <boost/circular_buffer.hpp>
#include <boost/container/static_vector.hpp>
#include <boost/stacktrace.hpp>
#include <boost/static_string.hpp>
#include <cassert>
#include <chrono>
#include <cstring>
#include <fileapi.h>
#include <fstream>
#include <iostream>
#include <list>
#include <mutex>
#include <pathcch.h>
#include <regex>
#include <shlobj_core.h>
#include <shlwapi.h>
#include <shobjidl_core.h>
#include <source_location>
#include <sstream>
#include <span>
#include <string_view>
#include <string.h>
#include <string>
#include <stringapiset.h>
#include <vector>
#include <windows.h>

#define IMGUI_DEFINE_MATH_OPERATORS 1
#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"
#include "imgui/font_awesome.h"
#include "imgui/material_design.h"

#define GL_SILENCE_DEPRECATION
#include <glfw3.h> // Will drag system OpenGL headers

#pragma warning(push)
#pragma warning(disable: 4244)
// #pragma warning(disable: 4459)
#include "libs/stb_image.h"
#pragma warning(pop)
