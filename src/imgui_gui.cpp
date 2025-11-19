/*
 * Dear ImGui GUI for TETRA Analyzer
 * Provides real-time control with OpenGL ES 2.0 fallback for ARM/Raspberry Pi
 *
 * This implementation uses Dear ImGui with GLFW3 and OpenGL
 * Supports OpenGL ES 2.0 (Raspberry Pi) with fallback to OpenGL 2.1+
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "tetra_analyzer.h"

#ifdef __cplusplus
}
#endif

#ifdef HAVE_IMGUI

// ImGui headers
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

// OpenGL + GLFW
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// GUI context structure
struct tetra_gui_t {
    // GLFW/OpenGL
    GLFWwindow* window;
    const char* glsl_version;

    // Shared state
    tetra_config_t *config;
    detection_params_t *params;
    detection_status_t *status;
    rtl_sdr_t *sdr;

    // GUI state
    bool running;
    bool show_stats_window;
    bool show_params_window;
    bool show_about_window;

    // Cached values for display
    float cached_signal_power;
    int cached_match_count;
    float cached_correlation;
    uint64_t cached_detection_count;
};

// Error callback for GLFW
static void glfw_error_callback(int error, const char* description) {
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

// Initialize ImGui GUI
extern "C" tetra_gui_t* tetra_gui_init(tetra_config_t *config, detection_params_t *params,
                                        detection_status_t *status, rtl_sdr_t *sdr) {
    tetra_gui_t *gui = (tetra_gui_t*)calloc(1, sizeof(tetra_gui_t));
    if (!gui) {
        fprintf(stderr, "Failed to allocate GUI context\n");
        return NULL;
    }

    gui->config = config;
    gui->params = params;
    gui->status = status;
    gui->sdr = sdr;
    gui->running = true;
    gui->show_stats_window = true;
    gui->show_params_window = true;
    gui->show_about_window = false;

    // Setup GLFW
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) {
        fprintf(stderr, "Failed to initialize GLFW\n");
        free(gui);
        return NULL;
    }

    // Decide GL+GLSL versions
    #if defined(__APPLE__)
        // GL 3.2 + GLSL 150
        gui->glsl_version = "#version 150";
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    #elif defined(__arm__) || defined(__aarch64__)
        // OpenGL ES 2.0 + GLSL 100 for ARM/Raspberry Pi
        gui->glsl_version = "#version 100";
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
        glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
    #else
        // GL 2.1 + GLSL 120 for older desktop systems
        gui->glsl_version = "#version 120";
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    #endif

    // Create window
    gui->window = glfwCreateWindow(900, 700, "TETRA Analyzer Control Panel", NULL, NULL);
    if (gui->window == NULL) {
        fprintf(stderr, "Failed to create GLFW window\n");
        glfwTerminate();
        free(gui);
        return NULL;
    }

    glfwMakeContextCurrent(gui->window);
    glfwSwapInterval(1); // Enable vsync

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Customize style for better visibility
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 5.0f;
    style.FrameRounding = 3.0f;
    style.GrabRounding = 3.0f;
    style.ScrollbarRounding = 3.0f;

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(gui->window, true);
    ImGui_ImplOpenGL3_Init(gui->glsl_version);

    log_message(true, "✓ ImGui GUI initialized (OpenGL version: %s)\n", gui->glsl_version);

    return gui;
}

// Run GUI main loop
extern "C" void tetra_gui_run(tetra_gui_t *gui) {
    if (!gui) return;

    ImVec4 clear_color = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);

    // Main loop
    while (!glfwWindowShouldClose(gui->window) && gui->running) {
        // Poll and handle events
        glfwPollEvents();

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Update cached status values
        if (gui->status) {
            pthread_mutex_lock(&gui->status->lock);
            gui->cached_signal_power = gui->status->current_signal_power;
            gui->cached_match_count = gui->status->last_match_count;
            gui->cached_correlation = gui->status->last_correlation;
            gui->cached_detection_count = gui->status->detection_count;
            pthread_mutex_unlock(&gui->status->lock);
        }

        // Menu bar
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("View")) {
                ImGui::MenuItem("Detection Parameters", NULL, &gui->show_params_window);
                ImGui::MenuItem("Status & Statistics", NULL, &gui->show_stats_window);
                ImGui::Separator();
                ImGui::MenuItem("About", NULL, &gui->show_about_window);
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Help")) {
                if (ImGui::MenuItem("About")) {
                    gui->show_about_window = true;
                }
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }

        // Detection Parameters Window
        if (gui->show_params_window) {
            ImGui::SetNextWindowSize(ImVec2(550, 500), ImGuiCond_FirstUseEver);
            ImGui::Begin("Detection Parameters", &gui->show_params_window);

            ImGui::Text("Real-time Parameter Control");
            ImGui::Separator();

            // Frequency control
            ImGui::Text("Frequency Control");
            static int freq_mhz = gui->config->frequency / 1000000;
            if (ImGui::SliderInt("Frequency (MHz)", &freq_mhz, 380, 470)) {
                gui->config->frequency = freq_mhz * 1000000;
            }
            ImGui::Text("Current: %u Hz (%.3f MHz)", gui->config->frequency, gui->config->frequency / 1e6);

            ImGui::Separator();
            ImGui::Text("Detection Thresholds");

            if (gui->params) {
                pthread_mutex_lock(&gui->params->lock);

                // Min Signal Power
                ImGui::SliderFloat("Min Signal Power", &gui->params->min_signal_power, 1.0f, 20.0f, "%.1f");
                ImGui::SameLine(); ImGui::TextDisabled("(?)");
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Minimum RMS signal power to accept.\nLower = more sensitive to weak signals.\nHigher = reject more noise.");
                }

                // Strong Match Threshold
                ImGui::SliderInt("Strong Match Threshold", &gui->params->strong_match_threshold, 18, 22, "%d/22 bits");
                ImGui::SameLine(); ImGui::TextDisabled("(?)");
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Primary detection threshold.\n20/22 = 90.9%% match required.\nHigher = fewer false positives.");
                }

                // Moderate Match Threshold
                ImGui::SliderInt("Moderate Match", &gui->params->moderate_match_threshold, 15, 22, "%d/22 bits");
                ImGui::SameLine(); ImGui::TextDisabled("(?)");
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Secondary threshold for weaker signals.\n19/22 = 86.4%% match.");
                }

                // Strong Correlation
                ImGui::SliderFloat("Strong Correlation", &gui->params->strong_correlation, 0.5f, 1.0f, "%.2f");
                ImGui::SameLine(); ImGui::TextDisabled("(?)");
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Correlation coefficient for strong matches.\nHigher = stricter pattern matching.");
                }

                // Moderate Correlation
                ImGui::SliderFloat("Moderate Correlation", &gui->params->moderate_correlation, 0.5f, 1.0f, "%.2f");

                // Low-Pass Filter
                ImGui::SliderFloat("Low-Pass Filter", &gui->params->lpf_cutoff, 0.1f, 1.0f, "%.2f");
                ImGui::SameLine(); ImGui::TextDisabled("(?)");
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Filter strength.\nLower = stronger filtering, more noise rejection.\nHigher = weaker filtering, faster response.");
                }

                // Power Multiplier
                ImGui::SliderFloat("Moderate Power Mult", &gui->params->moderate_power_multiplier, 1.0f, 2.0f, "%.2f");

                pthread_mutex_unlock(&gui->params->lock);

                ImGui::Separator();
                if (ImGui::Button("Reset to Defaults", ImVec2(200, 30))) {
                    detection_params_reset_defaults(gui->params);
                }
            }

            ImGui::End();
        }

        // Status & Statistics Window
        if (gui->show_stats_window) {
            ImGui::SetNextWindowSize(ImVec2(450, 350), ImGuiCond_FirstUseEver);
            ImGui::Begin("Status & Statistics", &gui->show_stats_window);

            ImGui::Text("Real-time Detection Status");
            ImGui::Separator();

            // Status indicator
            bool burst_detected = false;
            if (gui->status) {
                pthread_mutex_lock(&gui->status->lock);
                burst_detected = gui->status->burst_detected;
                pthread_mutex_unlock(&gui->status->lock);
            }

            if (burst_detected) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
                ImGui::Text("● BURST DETECTED");
                ImGui::PopStyleColor();
            } else if (gui->cached_signal_power > 1.0f) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.0f, 1.0f));
                ImGui::Text("● SIGNAL PRESENT");
                ImGui::PopStyleColor();
            } else {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
                ImGui::Text("● NO SIGNAL");
                ImGui::PopStyleColor();
            }

            ImGui::Separator();

            // Signal metrics
            ImGui::Text("Signal Power:");
            ImGui::SameLine(150);
            ImGui::Text("%.2f RMS", gui->cached_signal_power);
            ImGui::ProgressBar(gui->cached_signal_power / 20.0f, ImVec2(-1, 0));

            ImGui::Text("Last Match Count:");
            ImGui::SameLine(150);
            ImGui::Text("%d/22 bits", gui->cached_match_count);
            ImGui::ProgressBar(gui->cached_match_count / 22.0f, ImVec2(-1, 0));

            ImGui::Text("Last Correlation:");
            ImGui::SameLine(150);
            ImGui::Text("%.3f", gui->cached_correlation);
            ImGui::ProgressBar((gui->cached_correlation + 1.0f) / 2.0f, ImVec2(-1, 0));

            ImGui::Separator();

            ImGui::Text("Total Detections:");
            ImGui::SameLine(150);
            ImGui::Text("%llu", (unsigned long long)gui->cached_detection_count);

            ImGui::Separator();

            // Configuration info
            ImGui::Text("Configuration:");
            ImGui::BulletText("Frequency: %.3f MHz", gui->config->frequency / 1e6);
            ImGui::BulletText("Sample Rate: %u Hz", gui->config->sample_rate);
            ImGui::BulletText("Verbose: %s", gui->config->verbose ? "Yes" : "No");
            ImGui::BulletText("Trunking: %s", gui->config->enable_trunking ? "Enabled" : "Disabled");

            ImGui::End();
        }

        // About Window
        if (gui->show_about_window) {
            ImGui::SetNextWindowSize(ImVec2(500, 300), ImGuiCond_FirstUseEver);
            ImGui::Begin("About TETRA Analyzer", &gui->show_about_window);

            ImGui::Text("TETRA TEA1 Educational Security Research Toolkit");
            ImGui::Text("Version: %s", TETRA_ANALYZER_VERSION);
            ImGui::Separator();

            ImGui::TextWrapped("This software demonstrates publicly disclosed vulnerabilities in TETRA TEA1 encryption for educational and research purposes.");
            ImGui::Spacing();
            ImGui::TextWrapped("⚠️  FOR AUTHORIZED RESEARCH & EDUCATION ONLY");
            ImGui::TextWrapped("Use only on laboratory equipment you own or control.");
            ImGui::Spacing();

            ImGui::Separator();
            ImGui::Text("GUI: Dear ImGui with OpenGL");
            ImGui::Text("GLSL Version: %s", gui->glsl_version);
            ImGui::Text("Platform: %s",
                #if defined(__arm__) || defined(__aarch64__)
                    "ARM (Raspberry Pi optimized)"
                #else
                    "x86/x64"
                #endif
            );

            ImGui::End();
        }

        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(gui->window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w,
                     clear_color.z * clear_color.w, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(gui->window);
    }
}

// Update status (called from external thread)
extern "C" void tetra_gui_update_status(tetra_gui_t *gui) {
    // Status is automatically updated in the main loop
    (void)gui;
}

// Cleanup GUI
extern "C" void tetra_gui_cleanup(tetra_gui_t *gui) {
    if (!gui) return;

    // Cleanup ImGui
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    // Cleanup GLFW
    if (gui->window) {
        glfwDestroyWindow(gui->window);
    }
    glfwTerminate();

    free(gui);
}

#else // !HAVE_IMGUI

// Stub implementations when ImGui is not available
extern "C" tetra_gui_t* tetra_gui_init(tetra_config_t *config, detection_params_t *params,
                                        detection_status_t *status, rtl_sdr_t *sdr) {
    (void)config; (void)params; (void)status; (void)sdr;
    fprintf(stderr, "GUI support not compiled in. Install ImGui dependencies.\n");
    return NULL;
}

extern "C" void tetra_gui_run(tetra_gui_t *gui) {
    (void)gui;
}

extern "C" void tetra_gui_cleanup(tetra_gui_t *gui) {
    (void)gui;
}

extern "C" void tetra_gui_update_status(tetra_gui_t *gui) {
    (void)gui;
}

#endif // HAVE_IMGUI
