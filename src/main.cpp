#include "core/Application.hpp"
#include <memory> // For std::make_unique
// Removed unnecessary includes like SDL.h, imgui.h, project-specific headers for UI elements, etc.
// as main.cpp now only deals with the Application class lifecycle.

// Include SDL_main.h to enable SDL's WinMain wrapper on Windows
#include <SDL3/SDL_main.h>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#include <iostream>
#endif

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