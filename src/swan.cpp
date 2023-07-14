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

#include "on_scope_exit.hpp"
#include "primitives.hpp"
#include "common.hpp"

#include "util.cpp"
#include "pinned.cpp"
#include "explorer.cpp"

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
void set_window_icon(GLFWwindow *window)
{
    GLFWimage icon;
    icon.pixels = nullptr;
    icon.width = 0;
    icon.height = 0;

    int icon_width, icon_height, icon_channels;
    u8 *icon_pixels = stbi_load("swan.png", &icon_width, &icon_height, &icon_channels, STBI_rgb_alpha);

    auto cleanup_icon_pixels_routine = make_on_scope_exit([icon_pixels] {
        stbi_image_free(icon_pixels);
    });

    if (icon_pixels)
    {
        icon.pixels = icon_pixels;
        icon.width = icon_width;
        icon.height = icon_height;

        glfwSetWindowIcon(window, 1, &icon);
    }
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
    debug_log("\n%%%%%% init %%%%%%\n");

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

    set_window_icon(window);

    [[maybe_unused]] auto &io = ImGui::GetIO();

    explorer_options expl_opts = {};
    if (!expl_opts.load_from_disk()) {
        debug_log("explorer_options::load_from_disk failed, setting defaults");
        expl_opts.show_dotdot_dir = true;
    #if !defined(NDEBUG)
        expl_opts.show_debug_info = true;
        expl_opts.show_cwd_len = true;
    #endif
    }

    windows_options win_opts = {};
    if (!win_opts.load_from_disk()) {
        debug_log("windows_options::load_from_disk failed, setting defaults");
        win_opts.show_explorer_1 = true;
        win_opts.show_pinned = true;
    #if !defined(NDEBUG)
        win_opts.show_demo = true;
    #endif
    }

    {
        auto [success, num_pins_loaded] = load_pins_from_disk(expl_opts.dir_separator());
        if (!success) {
            debug_log("load_pins_from_disk failed");
        } else {
            debug_log("load_pins_from_disk success, loaded %zu pins", num_pins_loaded);
        }
    }

    std::vector<explorer_window> explorers(4);
    {
        char const *names[] = { "Explorer 1", "Explorer 2", "Explorer 3", "Explorer 4" };

        for (u64 i = 0; i < explorers.size(); ++i) {
            auto &expl = explorers[i];

            expl.name = names[i];
            expl.filter_error.reserve(1024);

            bool load_result = explorers[i].load_from_disk(expl_opts.dir_separator());
            debug_log("Explorer %zu load_from_disk result: %d", i+1, load_result);

            if (!load_result) {
                std::string startup_path_stdstr = std::filesystem::current_path().string();

                path_t startup_path = {};
                path_append(startup_path, startup_path_stdstr.c_str());
                path_force_separator(startup_path, expl_opts.dir_separator());

                expl.cwd = startup_path;
                expl.wd_history.push_back(startup_path);

                bool save_result = explorers[i].save_to_disk();
                debug_log("Explorer %zu save_to_disk result: %d", i+1, save_result);
            }

            update_cwd_entries(full_refresh, &expl, expl.cwd.data(), expl_opts);
        }
    }

    debug_log("\n%%%%%% render loop %%%%%%\n");

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::DockSpaceOverViewport(0, ImGuiDockNodeFlags_PassthruCentralNode);

        {
            ImGuiStyle &style = ImGui::GetStyle();
            f32 original_padding = style.FramePadding.y;

            style.FramePadding.y = 7.5f;

            if (ImGui::BeginMainMenuBar()) {
                if (ImGui::BeginMenu("[Windows]")) {
                    bool change_made = false;
                    static_assert((false | false) == false);
                    static_assert((false | true) == true);
                    static_assert((true | true) == true);

                    change_made |= ImGui::MenuItem("Pinned", nullptr, &win_opts.show_pinned);

                    change_made |= ImGui::MenuItem(explorers[0].name, nullptr, &win_opts.show_explorer_0);
                    change_made |= ImGui::MenuItem(explorers[1].name, nullptr, &win_opts.show_explorer_1);
                    change_made |= ImGui::MenuItem(explorers[2].name, nullptr, &win_opts.show_explorer_2);
                    change_made |= ImGui::MenuItem(explorers[3].name, nullptr, &win_opts.show_explorer_3);

                    change_made |= ImGui::MenuItem("Analytics", nullptr, &win_opts.show_analytics);
                    change_made |= ImGui::MenuItem("ImGui Demo", nullptr, &win_opts.show_demo);

                    ImGui::EndMenu();

                    if (change_made) {
                        bool result = win_opts.save_to_disk();
                        debug_log("windows_options::save_to_disk result: %d", result);
                    }
                }
                if (ImGui::BeginMenu("[Explorer Options]")) {
                    bool change_made = false;
                    static_assert((false | false) == false);
                    static_assert((false | true) == true);
                    static_assert((true | true) == true);

                    {
                        bool changed_dotdot_dir = ImGui::MenuItem("Show '..' directory", nullptr, &expl_opts.show_dotdot_dir);
                        if (changed_dotdot_dir) {
                            for (auto &expl : explorers) {
                                update_cwd_entries(full_refresh, &expl, expl.cwd.data(), expl_opts);
                            }
                        }
                        change_made |= changed_dotdot_dir;
                    }

                    change_made |= ImGui::MenuItem("Show cwd length", nullptr, &expl_opts.show_cwd_len);
                    change_made |= ImGui::MenuItem("Show debug info", nullptr, &expl_opts.show_debug_info);

                    {
                        bool changed_dir_separator = ImGui::MenuItem("Unix directory separators", nullptr, &expl_opts.unix_directory_separator);
                        if (changed_dir_separator) {
                            for (auto &expl : explorers) {
                                update_cwd_entries(full_refresh, &expl, expl.cwd.data(), expl_opts);
                            }
                            update_pin_dir_separators(expl_opts.dir_separator());
                        }
                        change_made |= changed_dir_separator;
                    }

                    change_made |= ImGui::MenuItem("Binary size system (1024 instead of 1000)", nullptr, &expl_opts.binary_size_system);

                    ImGui::EndMenu();

                    if (change_made) {
                        bool result = expl_opts.save_to_disk();
                        debug_log("explorer_options::save_to_disk result: %d", result);
                    }
                }
                ImGui::EndMainMenuBar();
            }

            style.FramePadding.y = original_padding;
        }

        if (win_opts.show_pinned) {
            render_pinned(explorers, win_opts, expl_opts);
        }

        if (win_opts.show_explorer_0) {
            render_file_explorer(explorers[0], expl_opts);
        }
        if (win_opts.show_explorer_1) {
            render_file_explorer(explorers[1], expl_opts);
        }
        if (win_opts.show_explorer_2) {
            render_file_explorer(explorers[2], expl_opts);
        }
        if (win_opts.show_explorer_3) {
            render_file_explorer(explorers[3], expl_opts);
        }

        if (win_opts.show_analytics) {
            if (ImGui::Begin("Analytics")) {
                ImGui::Text("%.1f FPS", io.Framerate);
                ImGui::Text("%.3f ms/frame ", 1000.0f / io.Framerate);
            }
            ImGui::End();
        }

        if (win_opts.show_demo) {
            ImGui::ShowDemoWindow();
        }

        render(window);
    }

    return 0;
}
