#include <filesystem>

#pragma warning(push)
#pragma warning(disable: 4244)
#define STB_IMAGE_IMPLEMENTATION
#include "stbi_image.h"
#pragma warning(pop)

#define IMGUI_DEFINE_MATH_OPERATORS 1
#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"

#include "on_scope_exit.cpp"
#include "primitives.cpp"
#include "options.cpp"
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
        window = glfwCreateWindow(screenWidth, screenHeight, "swan", nullptr, nullptr);
        if (window == nullptr) {
            return nullptr;
        }
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // vsync

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
    ImGui::StyleColorsClassic();

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
    debug_log("--------------------");

    GLFWwindow *window = init_glfw_and_imgui();
    if (window == nullptr) {
        return 1;
    }

    if (!init_windows_shell_com_garbage()) {
        return 1;
    }

    auto cleanup_routine = make_on_scope_exit([window]() {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        glfwDestroyWindow(window);
        glfwTerminate();
        cleanup_windows_shell_com_garbage();
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

    path_t starting_path = {};
    if (!GetCurrentDirectoryA((i32)starting_path.size(), starting_path.data())) {
        debug_log("GetCurrentDirectoryA failed");
    }

    static explorer_options expl_opts = {};
    expl_opts.show_dotdot_dir = true;
#if !defined(NDEBUG)
    expl_opts.show_debug_info = true;
    expl_opts.show_cwd_len = true;
#endif

    std::vector<explorer_window> explorers = {};
    explorers.reserve(4);
    explorers.emplace_back(create_default_explorer_windows("Explorer 1", true, starting_path, expl_opts));
    explorers.emplace_back(create_default_explorer_windows("Explorer 2", true, starting_path, expl_opts));
    explorers.emplace_back(create_default_explorer_windows("Explorer 3", false, starting_path, expl_opts));
    explorers.emplace_back(create_default_explorer_windows("Explorer 4", false, starting_path, expl_opts));

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::DockSpaceOverViewport(0, ImGuiDockNodeFlags_PassthruCentralNode);

        static bool show_analytics = false;
        static bool show_demo = false;

        {
            ImGuiStyle &style = ImGui::GetStyle();
            f32 original_padding = style.FramePadding.y;

            style.FramePadding.y = 7.5f;

            if (ImGui::BeginMainMenuBar()) {
                if (ImGui::BeginMenu("[Windows]")) {
                    for (auto &expl : explorers) {
                        ImGui::MenuItem(expl.name, nullptr, &expl.show);
                    }
                    ImGui::MenuItem("Analytics", nullptr, &show_analytics);
                    ImGui::MenuItem("ImGui Demo", nullptr, &show_demo);
                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu("[Explorer Options]")) {
                    ImGui::MenuItem("Binary size system (1024 instead of 1000)", nullptr, &expl_opts.binary_size_system);
                    if (ImGui::MenuItem("Show '..' directory", nullptr, &expl_opts.show_dotdot_dir)) {
                        for (auto &expl : explorers) {
                            update_cwd_entries(full_refresh, &expl, expl.cwd.data(), expl_opts);
                        }
                    }
                    ImGui::MenuItem("Show cwd length", nullptr, &expl_opts.show_cwd_len);
                    ImGui::MenuItem("Show debug info", nullptr, &expl_opts.show_debug_info);
                    ImGui::EndMenu();
                }
                ImGui::EndMainMenuBar();
            }

            style.FramePadding.y = original_padding;
        }

        for (auto &expl : explorers) {
            render_file_explorer(expl, expl_opts);
        }

        if (show_analytics) {
            if (ImGui::Begin("Analytics")) {
                ImGui::Text("%.1f FPS", io.Framerate);
                ImGui::Text("%.3f ms/frame ", 1000.0f / io.Framerate);
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
