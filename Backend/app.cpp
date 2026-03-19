#include "app.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GL/gl3w.h>
#include <GLFW/glfw3.h>
#include <stdio.h>

Application::Application() {
    dataLoader = std::make_unique<DataLoader>();
}

Application::~Application() {
    shutdown();
}

bool Application::initialize() {
    // Инициализация GLFW
    if (!glfwInit()) {
        fprintf(stderr, "Failed to initialize GLFW\n");
        return false;
    }
    
    // Настройка OpenGL
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    
    // Создание окна
    window = glfwCreateWindow(1280, 720, "Geophysics Data Visualizer", nullptr, nullptr);
    if (!window) {
        fprintf(stderr, "Failed to create GLFW window\n");
        glfwTerminate();
        return false;
    }
    
    glfwMakeContextCurrent((GLFWwindow*)window);
    glfwSwapInterval(1); // VSync
    
    // Инициализация OpenGL loader
    gl3wInit();
    
    // Настройка Dear ImGui
    IMGUI_CHECKVERSION();
    context = ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    
    // Стиль
    ImGui::StyleColorsDark();
    
    // Инициализация бэкендов
    ImGui_ImplGlfw_InitForOpenGL((GLFWwindow*)window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
    
    return true;
}

void Application::run() {
    while (!glfwWindowShouldClose((GLFWwindow*)window)) {
        glfwPollEvents();
        
        // Начало кадра ImGui
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        
        render();
        
        // Рендеринг
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize((GLFWwindow*)window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        
        glfwSwapBuffers((GLFWwindow*)window);
    }
}

void Application::render() {
    renderMenuBar();
    renderSidebar();
    renderViewport();
}

void Application::renderMenuBar() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Open Data File", "Ctrl+O")) {
                // Открыть файл
            }
            if (ImGui::MenuItem("Exit")) {
                glfwSetWindowShouldClose((GLFWwindow*)window, true);
            }
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Show Terminator", nullptr, &config.showTerminator);
            ImGui::MenuItem("Show Subsolar Point", nullptr, &config.showSubsolarPoint);
            ImGui::MenuItem("Show Equator", nullptr, &config.showEquator);
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("Help")) {
            ImGui::MenuItem("About");
            ImGui::EndMenu();
        }
        
        ImGui::EndMainMenuBar();
    }
}

void Application::renderSidebar() {
    ImGui::SetNextWindowSize(ImVec2(350, 0), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_FirstUseEver);
    
    if (ImGui::Begin("Controls", nullptr, ImGuiWindowFlags_NoCollapse)) {
        
        // Тип визуализации
        ImGui::SeparatorText("Visualization Type");
        const char* vizTypes[] = {
            "Scalar (Lat-Lon)",
            "Vector (Lat-Lon)",
            "Scalar (Polar)",
            "Vector (Polar)"
        };
        ImGui::Combo("##VizType", &config.vizType, vizTypes, IM_ARRAYSIZE(vizTypes));
        
        // Загрузка файла
        ImGui::SeparatorText("Data File");
        ImGui::InputText("Input File", &config.inputFile[0], config.inputFile.size());
        if (ImGui::Button("Load Data")) {
            loadData();
        }
        
        if (dataLoaded) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0, 1, 0, 1), "✓ Loaded");
        }
        
        // Область отображения
        ImGui::SeparatorText("Display Region");
        ImGui::SliderFloat("Lat Min", &config.latMin, -90.0f, 90.0f);
        ImGui::SliderFloat("Lat Max", &config.latMax, -90.0f, 90.0f);
        ImGui::SliderFloat("Lon Min", &config.lonMin, -180.0f, 180.0f);
        ImGui::SliderFloat("Lon Max", &config.lonMax, -180.0f, 180.0f);
        
        // Overlay elements
        ImGui::SeparatorText("Overlay Elements");
        ImGui::Checkbox("Show Terminator", &config.showTerminator);
        ImGui::Checkbox("Show Subsolar Point", &config.showSubsolarPoint);
        ImGui::Checkbox("Show Geographic Equator", &config.showEquator);
        ImGui::Checkbox("Show Magnetic Equator", &config.showMagneticEquator);
        ImGui::Checkbox("Show MLT Grid", &config.showMLT);
        
        // Color scheme
        ImGui::SeparatorText("Color Scheme");
        const char* schemes[] = { "Grayscale", "Blue-Green-Red", "Rainbow" };
        ImGui::Combo("##ColorScheme", &config.colorScheme, schemes, IM_ARRAYSIZE(schemes));
        
        // Value range
        ImGui::SeparatorText("Value Range");
        ImGui::Checkbox("Auto Scale", &config.autoScale);
        if (!config.autoScale) {
            ImGui::SliderFloat("Min Value", &config.valueMin, 0.0f, 100.0f);
            ImGui::SliderFloat("Max Value", &config.valueMax, 0.0f, 100.0f);
        }
        
        // Contours
        ImGui::SeparatorText("Contours");
        ImGui::Checkbox("Show Contours", &config.showContours);
        if (config.showContours) {
            ImGui::SliderFloat("Interval", &config.contourInterval, 1.0f, 50.0f);
        }
        
        // Advanced options
        ImGui::SeparatorText("Advanced");
        ImGui::Checkbox("Center on Noon Meridian", &config.centerOnNoon);
        ImGui::Checkbox("Mirror South Pole", &config.mirrorSouthPole);
        ImGui::Checkbox("Logarithmic Scale", &config.useLogScale);
        
        // Apply button
        ImGui::Separator();
        if (ImGui::Button("Update Visualization", ImVec2(-1, 30))) {
            updateVisualization();
        }
        
    }
    ImGui::End();
}

void Application::renderViewport() {
    ImGui::SetNextWindowPos(ImVec2(350, 0), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(930, 720), ImGuiCond_FirstUseEver);
    
    if (ImGui::Begin("Viewport")) {
        if (!dataLoaded) {
            ImGui::SetCursorPos(ImVec2(
                ImGui::GetWindowSize().x * 0.5f - 100,
                ImGui::GetWindowSize().y * 0.5f
            ));
            ImGui::Text("No data loaded. Please load a data file.");
        } else {
            // Здесь будет рендеринг визуализации
            ImGui::Image((void*)(intptr_t)0, ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y));
            
            // Временная заглушка
            ImGui::SetCursorPos(ImVec2(10, 10));
            ImGui::Text("Visualization Area");
            ImGui::SetCursorPos(ImVec2(10, 30));
            ImGui::Text("Type: %d", config.vizType);
        }
    }
    ImGui::End();
}

void Application::loadData() {
    if (config.inputFile.empty()) {
        fprintf(stderr, "No input file specified\n");
        return;
    }
    
    // dataLoader->load(config.inputFile);
    dataLoaded = true;
}

void Application::updateVisualization() {
    // Логика обновления визуализации
    printf("Updating visualization with config:\n");
    printf("  Type: %d\n", config.vizType);
    printf("  Region: [%.1f, %.1f] x [%.1f, %.1f]\n", 
           config.latMin, config.latMax, config.lonMin, config.lonMax);
}

void Application::shutdown() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext((ImGuiContext*)context);
    
    glfwDestroyWindow((GLFWwindow*)window);
    glfwTerminate();
}