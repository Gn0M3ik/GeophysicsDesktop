#pragma once
#include <string>
#include <vector>
#include <memory>
#include "data_loader.h"

struct AppConfig {
    // Тип визуализации
    int vizType = 0; // 0 - скаляр, 1 - вектор, 2 - полярный скаляр, 3 - полярный вектор
    
    // Параметры отображения
    bool showTerminator = true;
    bool showSubsolarPoint = true;
    bool showEquator = true;
    bool showMagneticEquator = false;
    bool showMLT = false;
    
    // Диапазоны
    float latMin = -90.0f;
    float latMax = 90.0f;
    float lonMin = -180.0f;
    float lonMax = 180.0f;
    
    // Цветовая схема
    int colorScheme = 0; // 0 - черно-белый, 1 - синий-зеленый-красный, 2 - цветной
    
    // Шкала
    bool useLogScale = false;
    float valueMin = 0.0f;
    float valueMax = 100.0f;
    bool autoScale = true;
    
    // Изолинии
    bool showContours = false;
    float contourInterval = 10.0f;
    
    // Центрирование
    bool centerOnNoon = false;
    bool mirrorSouthPole = false;
    
    // Путь к файлу
    std::string inputFile;
};

class Application {
public:
    Application();
    ~Application();
    
    bool initialize();
    void run();
    void shutdown();
    
private:
    void render();
    void renderMenuBar();
    void renderSidebar();
    void renderViewport();
    void renderFileLoader();
    
    void loadData();
    void updateVisualization();
    
    // Данные
    AppConfig config;
    std::unique_ptr<DataLoader> dataLoader;
    bool dataLoaded = false;
    
    // Окно и контекст (будут инициализированы)
    void* window = nullptr;
    void* context = nullptr;
};