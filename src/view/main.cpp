#include "core/Application.hpp"
#include <iostream>

int main(int argc, char* argv[])
{
    // Create and initialize the application
    Application app;
    
    if (!app.Initialize(argc, argv))
    {
        std::cerr << "Failed to initialize application" << std::endl;
        return 1;
    }
    
    // Run the application
    int result = app.Run();
    
    // Clean up
    app.Shutdown();
    
    return result;
} 