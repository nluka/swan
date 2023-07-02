#define _CRT_SECURE_NO_WARNINGS
#include <cstdio>
#include <string>
#include <cstring>
#include <memory>

#if 0
    // [Win32] Our example includes a copy of glfw3.lib pre-compiled with VS2010 to maximize ease of testing and compatibility with old VS compilers.
    // To link with VS2010-era libraries, VS2015+ requires linking with legacy_stdio_definitions.lib, which we do using this pragma.
    // Your own project should not be affected, as you are likely to link with a newer binary of GLFW that is adequate for your version of Visual Studio.
    #if defined(_MSC_VER) && (_MSC_VER >= 1900) && !defined(IMGUI_DISABLE_WIN32_FUNCTIONS)
    #pragma comment(lib, "legacy_stdio_definitions")
    #endif
#endif

#pragma warning(push)
#pragma warning(disable: 4244)
#define STB_IMAGE_IMPLEMENTATION
#include "stbi_image.h"
#pragma warning(pop)

#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"

#include "on_scope_exit.hpp"
#include "primitives.hpp"
#include "options.hpp"

#include "explorer.cpp"
#include "util.cpp"

#define GL_SILENCE_DEPRECATION
#include <glfw3.h> // Will drag system OpenGL headers

static
void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

static
GLFWwindow *init_glfw_and_imgui()
{
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) {
        return nullptr;
    }

    // GL 3.0 + GLSL 130
    {
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
        //glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
        //glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only
    }

    GLFWwindow *window = nullptr;
    // Create window with graphics context
    {
        int screenWidth = GetSystemMetrics(SM_CXSCREEN);
        int screenHeight = GetSystemMetrics(SM_CYSCREEN);
        window = glfwCreateWindow(screenWidth, screenHeight, "nexplorer", nullptr, nullptr);
        if (window == nullptr) {
            return nullptr;
        }
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    io.ConfigDockingWithShift = true;
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::SetNextWindowSizeConstraints(io.DisplaySize, io.DisplaySize);
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    {
        char const *glsl_version = "#version 130";
        ImGui_ImplGlfw_InitForOpenGL(window, true);
        ImGui_ImplOpenGL3_Init(glsl_version);
    }

    {
        [[maybe_unused]] auto font = io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/consola.ttf", 15.0f);
        // IM_ASSERT(font != nullptr);

        io.Fonts->AddFontDefault();
    }

    return window;
}

static
void render(GLFWwindow *window)
{
    ImGui::Render();

    int display_w, display_h;
    ImVec4 clear_color(0.45f, 0.55f, 0.60f, 1.00f);

    glfwGetFramebufferSize(window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(window);
}

i32 main(i32, char**)
{
    GLFWwindow *window = init_glfw_and_imgui();
    if (window == nullptr) {
        return 1;
    }

    auto window_cleanup_routine = make_on_scope_exit([window]() {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        glfwDestroyWindow(window);
        glfwTerminate();
    });

    [[maybe_unused]] auto &io = ImGui::GetIO();

    {
        GLFWimage icon;
        icon.pixels = nullptr;
        icon.width = 0;
        icon.height = 0;

        int icon_width, icon_height, icon_channels;
        u8 *icon_pixels = stbi_load("swan.png", &icon_width, &icon_height, &icon_channels, STBI_rgb_alpha);

        if (icon_pixels)
        {
            icon.pixels = icon_pixels;
            icon.width = icon_width;
            icon.height = icon_height;

            glfwSetWindowIcon(window, 1, &icon);
        }

        stbi_image_free(icon_pixels);
    }

    std::vector<explorer_window> explorers = {};
    explorers.reserve(2);

    explorers.push_back({});
    explorers.push_back({});

    {
        explorer_window &explorer = explorers[0];
        explorer.show = true;
        explorer.dir_entries.reserve(1024);
        explorer.name = "Explorer 1";
        explorer.last_selected_dirent_idx = explorer_window::NO_SELECTION;
        if (0 == GetCurrentDirectoryA((i32)explorer.working_dir.size(), explorer.working_dir.data())) {
            debug_log("%s: GetCurrentDirectoryA failed", explorer.name);
        } else {
            update_dir_entries(&explorer, explorer.working_dir.data());
        }
    }
    {
        explorer_window &explorer = explorers[1];
        explorer.show = true;
        explorer.dir_entries.reserve(1024);
        explorer.name = "Explorer 2";
        explorer.last_selected_dirent_idx = explorer_window::NO_SELECTION;
        if (0 == GetCurrentDirectoryA((i32)explorer.working_dir.size(), explorer.working_dir.data())) {
            debug_log("%s: GetCurrentDirectoryA failed", explorer.name);
        } else {
            update_dir_entries(&explorer, explorer.working_dir.data());
        }
    }

    static bool show_analytics = false;
    static bool show_demo = false;

    explorer_options expl_opts = {};

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::DockSpaceOverViewport(0, ImGuiDockNodeFlags_PassthruCentralNode);

        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("[Windows]")) {
                for (auto &expl : explorers) {
                    ImGui::MenuItem(expl.name, nullptr, &expl.show);
                }
                ImGui::MenuItem("Analytics", NULL, &show_analytics);
                ImGui::MenuItem("ImGui Demo", NULL, &show_demo);
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("[Explorer Options]")) {
                ImGui::MenuItem("Show cwd length", NULL, &expl_opts.show_cwd_len);
                ImGui::MenuItem("Show debug info", NULL, &expl_opts.show_debug_info);
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }

        for (auto &expl : explorers) {
            render_file_explorer(expl, expl_opts);
        }

        if (show_analytics) {
            if (ImGui::Begin("Analytics")) {
                ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
            }
            ImGui::End();
        }

        if (show_demo) {
            ImGui::ShowDemoWindow();
        }

        render(window);
    }

    return 0;
}
