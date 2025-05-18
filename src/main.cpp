#include "core/Application.hpp"
#include <memory> // For std::make_unique
// Removed unnecessary includes like SDL.h, imgui.h, project-specific headers for UI elements, etc.
// as main.cpp now only deals with the Application class lifecycle.

int main(int argc, char* args[]) {
    // Suppress unused parameter warnings if argc and args are not used.
    (void)argc;
    (void)args;

    auto app = std::make_unique<Application>();

    if (!app->Initialize()) {
        // Initialization failed, Application::Initialize should have logged details.
        return 1;
    }

    app->Run();

    app->Shutdown();

    return 0;
} 