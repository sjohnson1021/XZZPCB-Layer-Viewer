#include "ui/interaction/NavigationTool.hpp"
#include "view/Camera.hpp"
#include "view/Viewport.hpp"
#include <cmath> // For M_PI if not already included by camera/viewport

// Define PI if not available
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

NavigationTool::NavigationTool(std::shared_ptr<Camera> camera, std::shared_ptr<Viewport> viewport)
    : InteractionTool("Navigation", camera, viewport) {}

void NavigationTool::ProcessInput(ImGuiIO& io, bool isViewportFocused, bool isViewportHovered, ImVec2 viewportTopLeft, ImVec2 viewportSize) {
    // Input should only be processed if the viewport (content region) is active
    // isViewportFocused means the ImGui window has focus.
    // isViewportHovered means the mouse is over the specific content region where rendering happens.
    if (!isViewportHovered && !isViewportFocused) return; // Only process if viewport is hovered or the window has focus (for keyboard input)
    if (!m_camera || !m_viewport) return;

    // Update viewport with current dimensions from the window
    // This is crucial because InteractionTool might be used by multiple viewports in theory
    // For now, it's tied to one, but good practice to ensure it has current dimensions.
    // The viewport's screen X,Y should be 0,0 if its coordinates are relative to the texture/content area.
    m_viewport->SetDimensions(0, 0, static_cast<int>(viewportSize.x), static_cast<int>(viewportSize.y));

    // Zooming with mouse wheel (only if hovered over the content area)
    if (io.MouseWheel != 0.0f && isViewportHovered) {
        float zoomSensitivity = 1.1f;
        float zoomFactor = (io.MouseWheel > 0.0f) ? zoomSensitivity : 1.0f / zoomSensitivity;

        ImVec2 mousePosAbsolute = ImGui::GetMousePos();
        // viewportTopLeft is the screen coordinate of the top-left of our render area (ImGui::Image)
        Vec2 mousePosInViewport = {mousePosAbsolute.x - viewportTopLeft.x,
                                     mousePosAbsolute.y - viewportTopLeft.y};

        Vec2 worldPosUnderMouse = m_viewport->ScreenToWorld(mousePosInViewport, *m_camera);
        float oldZoom = m_camera->GetZoom();
        m_camera->SetZoom(oldZoom * zoomFactor);
        float newZoom = m_camera->GetZoom();

        Vec2 camPos = m_camera->GetPosition();
        if (newZoom != 0.0f && oldZoom != 0.0f) { // Avoid division by zero
            m_camera->SetPosition(camPos + (worldPosUnderMouse - camPos) * (1.0f - oldZoom / newZoom));
        }
    }

    // Panning (Middle Mouse Button or Right Mouse Button + Drag, only if hovered)
    if (isViewportHovered && (ImGui::IsMouseDragging(ImGuiMouseButton_Middle) || ImGui::IsMouseDragging(ImGuiMouseButton_Right))) {
        ImVec2 delta = io.MouseDelta;
        if (delta.x != 0.0f || delta.y != 0.0f) {
            Vec2 worldDelta = m_viewport->ScreenDeltaToWorldDelta({delta.x, delta.y}, *m_camera);
            m_camera->Pan(-worldDelta);
        }
    }

    // Keyboard controls (active if window is focused)
    if (isViewportFocused) {
        float panSpeed = 100.0f / m_camera->GetZoom(); // World units per second
        panSpeed = std::max(1.0f, panSpeed); // Ensure a minimum pan speed at high zoom
        float rotationSpeed = 90.0f; // Degrees per second

        if (ImGui::IsKeyDown(ImGuiKey_LeftArrow) || ImGui::IsKeyDown(ImGuiKey_A)) m_camera->Pan({-panSpeed * io.DeltaTime, 0.0f});
        if (ImGui::IsKeyDown(ImGuiKey_RightArrow) || ImGui::IsKeyDown(ImGuiKey_D)) m_camera->Pan({panSpeed * io.DeltaTime, 0.0f});
        if (ImGui::IsKeyDown(ImGuiKey_UpArrow) || ImGui::IsKeyDown(ImGuiKey_W)) m_camera->Pan({0.0f, panSpeed * io.DeltaTime});
        if (ImGui::IsKeyDown(ImGuiKey_DownArrow) || ImGui::IsKeyDown(ImGuiKey_S)) m_camera->Pan({0.0f, -panSpeed * io.DeltaTime});
        
        // Zoom with +/- keys
        float keyboardZoomFactor = 1.0f + (2.0f * io.DeltaTime); // Zoom factor per second
        if (ImGui::IsKeyDown(ImGuiKey_Equal) || ImGui::IsKeyDown(ImGuiKey_KeypadAdd)) m_camera->AdjustZoom(keyboardZoomFactor);
        if (ImGui::IsKeyDown(ImGuiKey_Minus) || ImGui::IsKeyDown(ImGuiKey_KeypadSubtract)) m_camera->AdjustZoom(1.0f/keyboardZoomFactor);

        // Rotation with Q/E keys
        if (ImGui::IsKeyDown(ImGuiKey_Q)) m_camera->SetRotation(m_camera->GetRotation() + rotationSpeed * io.DeltaTime);
        if (ImGui::IsKeyDown(ImGuiKey_E)) m_camera->SetRotation(m_camera->GetRotation() - rotationSpeed * io.DeltaTime);

        // Reset View (R key)
        if (ImGui::IsKeyPressed(ImGuiKey_R, false)) { // `false` to allow repeat if key is held (ImGui default is true)
            m_camera->Reset();
        }
    }
} 