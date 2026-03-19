#include "app.h"
#include <iostream>

int main(int argc, char* argv[]) {
    std::cout << "Geophysics Data Visualizer v1.0\n";
    std::cout << "================================\n\n";
    
    Application app;
    
    if (!app.initialize()) {
        std::cerr << "Failed to initialize application\n";
        return -1;
    }
    
    std::cout << "Application initialized successfully\n";
    std::cout << "Starting main loop...\n";
    
    app.run();
    
    std::cout << "Application shutdown\n";
    return 0;
}