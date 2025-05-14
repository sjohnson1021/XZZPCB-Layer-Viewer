// Dear ImGui: standalone example application for SDL3 + SDL_Renderer
// (SDL is a cross-platform general purpose library for handling windows, inputs, OpenGL/Vulkan/Metal graphics context creation, etc.)

// Learn about Dear ImGui:
// - FAQ                  https://dearimgui.com/faq
// - Getting Started      https://dearimgui.com/getting-started
// - Documentation        https://dearimgui.com/docs (same as your local docs/ folder).
// - Introduction, links and more at the top of imgui.cpp

// Important to understand: SDL_Renderer is an _optional_ component of SDL3.
// For a multi-platform app consider using e.g. SDL+DirectX on Windows and SDL+OpenGL on Linux/OSX.

#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlrenderer3.h"
#include <stdio.h>
#include <SDL3/SDL.h>

#ifdef __EMSCRIPTEN__
#include "../libs/emscripten/emscripten_mainloop_stub.h"
#endif

// Viewport data and state
struct ViewportData {
    float clearColor[4] = {0.2f, 0.2f, 0.2f, 1.0f}; // Background color for content area
    SDL_Texture* contentTexture = nullptr; // Optional texture for content area
    ImVec2 contentAreaSize = {0, 0}; // Size of the content drawing area
    ImVec2 contentAreaPos = {0, 0};  // Position of the content drawing area
};

// Main code
int main(int, char**)
{
    // Setup SDL
    // [If using SDL_MAIN_USE_CALLBACKS: all code below until the main loop starts would likely be your SDL_AppInit() function]
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
    {
        printf("Error: SDL_Init(): %s\n", SDL_GetError());
        return -1;
    }

    // Create window with SDL_Renderer graphics context
    SDL_WindowFlags window_flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    SDL_Window* window = SDL_CreateWindow("PCB Viewer", 1280, 720, window_flags);
    if (window == nullptr)
    {
        printf("Error: SDL_CreateWindow(): %s\n", SDL_GetError());
        return -1;
    }
    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    SDL_SetRenderVSync(renderer, 1);
    if (renderer == nullptr)
    {
        SDL_Log("Error: SDL_CreateRenderer(): %s\n", SDL_GetError());
        return -1;
    }
    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_ShowWindow(window);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Enable Docking

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    // Setup Platform/Renderer backends
    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return a nullptr. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
    // - Use '#define IMGUI_ENABLE_FREETYPE' in your imconfig file to use Freetype for higher quality font rendering.
    // - Read 'docs/FONTS.md' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    // - Our Emscripten build process allows embedding fonts to be accessible at runtime from the "fonts/" folder. See Makefile.emscripten for details.
    //io.Fonts->AddFontDefault();
    //io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf", 18.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
    //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, nullptr, io.Fonts->GetGlyphRangesJapanese());
    //IM_ASSERT(font != nullptr);

    // Our application state
    ViewportData viewportData;
    bool show_demo_window = true;
    bool show_layer_controls = true;

    // Main loop
    bool done = false;
#ifdef __EMSCRIPTEN__
    // For an Emscripten build we are disabling file-system access, so let's not attempt to do a fopen() of the imgui.ini file.
    // You may manually call LoadIniSettingsFromMemory() to load settings from your own storage.
    io.IniFilename = nullptr;
    EMSCRIPTEN_MAINLOOP_BEGIN
#else
    while (!done)
#endif
    {
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        // [If using SDL_MAIN_USE_CALLBACKS: call ImGui_ImplSDL3_ProcessEvent() from your SDL_AppEvent() function]
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT)
                done = true;
            if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(window))
                done = true;
        }

        // [If using SDL_MAIN_USE_CALLBACKS: all code below would likely be your SDL_AppIterate() function]
        if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED)
        {
            SDL_Delay(10);
            continue;
        }

        // Start the Dear ImGui frame
        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        // Create main dockspace
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(viewport->Size);
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
        window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
        
        ImGui::Begin("DockSpace", nullptr, window_flags);
        ImGui::PopStyleVar(2);
        
        // DockSpace
        ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f));
        
        // Main Menu Bar
        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::MenuItem("Open PCB...", "Ctrl+O")) {}
                ImGui::Separator();
                if (ImGui::MenuItem("Exit", "Alt+F4")) { done = true; }
                ImGui::EndMenu();
            }
            
            if (ImGui::BeginMenu("View"))
            {
                ImGui::MenuItem("Demo Window", nullptr, &show_demo_window);
                ImGui::MenuItem("Layer Controls", nullptr, &show_layer_controls);
                ImGui::EndMenu();
            }
            
            if (ImGui::BeginMenu("Help"))
            {
                if (ImGui::MenuItem("About...")) {}
                ImGui::EndMenu();
            }
            
            ImGui::EndMenuBar();
        }
        
        ImGui::End(); // DockSpace

        // Show demo window if enabled
        if (show_demo_window)
            ImGui::ShowDemoWindow(&show_demo_window);

        // Create a PCB View window that will be our main content area
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::Begin("PCB View", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        
        // Get the content area size and position
        viewportData.contentAreaPos = ImGui::GetCursorScreenPos();
        viewportData.contentAreaSize = ImGui::GetContentRegionAvail();
        
        // Add a dummy element that fills the entire window
        ImGui::InvisibleButton("pcb_canvas", viewportData.contentAreaSize);
        
        // Handle mouse interactions in the PCB view
        if (ImGui::IsItemHovered())
        {
            // Handle mouse events for PCB view here
            ImVec2 mousePos = ImGui::GetMousePos();
            ImVec2 canvasPos = ImGui::GetItemRectMin();
            ImVec2 localPos = ImVec2(mousePos.x - canvasPos.x, mousePos.y - canvasPos.y);
            
            if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
            {
                // Handle left mouse button down
            }
            
            // Mouse wheel for zooming
            if (io.MouseWheel != 0)
            {
                // Handle zoom
            }
        }
        
        ImGui::End(); // PCB View
        ImGui::PopStyleVar();
        
        // Layer Controls window
        if (show_layer_controls)
        {
            ImGui::Begin("Layer Controls", &show_layer_controls);
            ImGui::Text("PCB Layers");
            ImGui::Separator();
            
            // Example layer toggles
            static bool top_copper = true;
            static bool bottom_copper = true;
            static bool top_silkscreen = true;
            static bool bottom_silkscreen = true;
            
            ImGui::Checkbox("Top Copper", &top_copper);
            ImGui::Checkbox("Bottom Copper", &bottom_copper);
            ImGui::Checkbox("Top Silkscreen", &top_silkscreen);
            ImGui::Checkbox("Bottom Silkscreen", &bottom_silkscreen);
            
            ImGui::Separator();
            ImGui::ColorEdit3("Background", viewportData.clearColor);
            
            ImGui::End();
        }

        // Rendering
        ImGui::Render();
        
        // Clear the entire screen with content area color
        SDL_SetRenderDrawColorFloat(renderer, 
            viewportData.clearColor[0], 
            viewportData.clearColor[1], 
            viewportData.clearColor[2], 
            viewportData.clearColor[3]);
        SDL_RenderClear(renderer);
        
        // Draw in the PCB View window
        if (viewportData.contentAreaSize.x > 0 && viewportData.contentAreaSize.y > 0)
        {
            // Draw a grid pattern in the PCB view area
            SDL_FRect gridRect = { 
                viewportData.contentAreaPos.x, 
                viewportData.contentAreaPos.y, 
                viewportData.contentAreaSize.x, 
                viewportData.contentAreaSize.y 
            };
            
            // Draw grid lines
            SDL_SetRenderDrawColorFloat(renderer, 0.4f, 0.4f, 0.4f, 1.0f);
            
            float gridSize = 20.0f;
            for (float x = gridRect.x; x < gridRect.x + gridRect.w; x += gridSize)
            {
                SDL_FRect lineRect = {x, gridRect.y, 1.0f, gridRect.h};
                SDL_RenderFillRect(renderer, &lineRect);
            }
            
            for (float y = gridRect.y; y < gridRect.y + gridRect.h; y += gridSize)
            {
                SDL_FRect lineRect = {gridRect.x, y, gridRect.w, 1.0f};
                SDL_RenderFillRect(renderer, &lineRect);
            }
            
            // Draw a placeholder PCB outline
            SDL_SetRenderDrawColorFloat(renderer, 0.0f, 0.8f, 0.0f, 1.0f);
            SDL_FRect pcbRect = {
                viewportData.contentAreaPos.x + viewportData.contentAreaSize.x * 0.2f,
                viewportData.contentAreaPos.y + viewportData.contentAreaSize.y * 0.2f,
                viewportData.contentAreaSize.x * 0.6f,
                viewportData.contentAreaSize.y * 0.6f
            };
            SDL_RenderRect(renderer, &pcbRect);
        }
        
        // Finally render ImGui on top
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
        SDL_RenderPresent(renderer);
    }
#ifdef __EMSCRIPTEN__
    EMSCRIPTEN_MAINLOOP_END;
#endif

    // Cleanup
    // [If using SDL_MAIN_USE_CALLBACKS: all code below would likely be your SDL_AppQuit() function]
    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
