#include "ui/interaction/NavigationTool.hpp"
#include "view/Camera.hpp"
#include "view/Viewport.hpp"
#include "core/ControlSettings.hpp"
#include "core/InputActions.hpp"     // Required for InputAction and KeyCombination
#include "core/BoardDataManager.hpp" // Added include
#include "pcb/Board.hpp"             // Added include for BLRect and Board methods
#include <cmath>                     // For M_PI if not already included by camera/viewport
#include <algorithm>                 // for std::max if not already there
#include <iostream>                  // for std::cout and std::endl

// Define PI if not available
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

inline float DegToRad(float degrees)
{
    return degrees * (static_cast<float>(M_PI) / 180.0f);
}

// Helper function to check if a keybind is active
bool IsKeybindActive(const KeyCombination &kb, ImGuiIO &io, bool useIsKeyPressed)
{
    if (!kb.IsBound())
        return false;

    bool keyStateCorrect;
    if (useIsKeyPressed)
    {
        keyStateCorrect = ImGui::IsKeyPressed(kb.key, false);
    }
    else
    {
        keyStateCorrect = ImGui::IsKeyDown(kb.key);
    }
    if (!keyStateCorrect)
        return false;

    // Check modifiers
    if (kb.ctrl && !io.KeyCtrl)
        return false;
    if (kb.shift && !io.KeyShift)
        return false;
    if (kb.alt && !io.KeyAlt)
        return false;

    // If we require exact modifiers (e.g. Ctrl must be down, others must be up)
    // if (!kb.ctrl && io.KeyCtrl) return false; // etc.
    // For now, we just check if required modifiers are pressed.

    return true;
}

NavigationTool::NavigationTool(std::shared_ptr<Camera> camera,
                               std::shared_ptr<Viewport> viewport,
                               std::shared_ptr<ControlSettings> controlSettings,
                               std::shared_ptr<BoardDataManager> boardDataManager)
    : InteractionTool("Navigation", camera, viewport), m_controlSettings(controlSettings), m_boardDataManager(boardDataManager) {}

void NavigationTool::ProcessInput(ImGuiIO &io, bool isViewportFocused, bool isViewportHovered, ImVec2 viewportTopLeft, ImVec2 viewportSize)
{
    // Optimize for static

    if (!isViewportHovered && !isViewportFocused)
        return; // Only process if viewport is hovered or the window has focus (for keyboard input)
    if (!m_camera || !m_viewport || !m_controlSettings)
        return;

    // Update viewport with current dimensions from the window
    // This is crucial because InteractionTool might be used by multiple viewports in theory
    // For now, it's tied to one, but good practice to ensure it has current dimensions.
    // The viewport's screen X,Y should be 0,0 if its coordinates are relative to the texture/content area.
    m_viewport->SetDimensions(0, 0, static_cast<int>(std::round(viewportSize.x)), static_cast<int>(std::round(viewportSize.y)));

    // Zooming with mouse wheel (only if hovered over the content area)
    if (io.MouseWheel != 0.0f && isViewportHovered)
    {
        float zoomSensitivity = 1.1f;
        float zoomFactor = (io.MouseWheel > 0.0f) ? zoomSensitivity : 1.0f / zoomSensitivity;

        ImVec2 mousePosAbsolute = ImGui::GetMousePos();
        // viewportTopLeft is the screen coordinate of the top-left of our render area (ImGui::Image)
        Vec2 mousePosInViewport = {mousePosAbsolute.x - viewportTopLeft.x,
                                   mousePosAbsolute.y - viewportTopLeft.y};

        Vec2 worldPosUnderMouse = m_viewport->ScreenToWorld(mousePosInViewport, *m_camera);
        float oldZoom_val = m_camera->GetZoom();
        m_camera->SetZoom(oldZoom_val * zoomFactor);
        float newZoom_val = m_camera->GetZoom();
        Vec2 camPos_val = m_camera->GetPosition();
        if (newZoom_val != 0.0f && oldZoom_val != 0.0f)
        {
            Vec2 newPos = camPos_val + (worldPosUnderMouse - camPos_val) * (1.0f - oldZoom_val / newZoom_val);
            m_camera->SetPosition(newPos);
            // debug print newpos
            std::cout << "newPos: " << newPos.x << ", " << newPos.y << std::endl;
        }
    }

    // Panning (Middle Mouse Button or Right Mouse Button + Drag, only if hovered)
    if (isViewportHovered && (ImGui::IsMouseDragging(ImGuiMouseButton_Middle) || ImGui::IsMouseDragging(ImGuiMouseButton_Right)))
    {
        ImVec2 delta = io.MouseDelta;
        if (delta.x != 0.0f || delta.y != 0.0f)
        {
            Vec2 worldDelta = m_viewport->ScreenDeltaToWorldDelta({delta.x, delta.y}, *m_camera);
            m_camera->Pan(worldDelta);
        }
    }

    // Keyboard controls (active if window is focused)
    if (isViewportFocused)
    {
        float panSpeed = 100.0f / m_camera->GetZoom();                   // World units per second
        panSpeed = std::max(1.0f, panSpeed);                             // Ensure a minimum pan speed at high zoom
        float freeRotationSpeedDegrees = 90.0f;                          // Degrees per second (for free rotation)
        float snapAngleDegrees = m_controlSettings->m_snapRotationAngle; // Degrees for snap rotation

        // Pan controls (now relative to camera rotation)
        Vec2 panInputAccumulator = {0.0f, 0.0f};
        bool isPanning = false;
        float effectivePanSpeed = panSpeed * io.DeltaTime;

        if (IsKeybindActive(m_controlSettings->GetKeybind(InputAction::PanUp), io, false))
        {
            panInputAccumulator.y += effectivePanSpeed; // Positive Y for local "up" (scene moves up, camera moves down - Y increases in Y-Down world)
            isPanning = true;
        }
        if (IsKeybindActive(m_controlSettings->GetKeybind(InputAction::PanDown), io, false))
        {
            panInputAccumulator.y -= effectivePanSpeed; // Negative Y for local "down" (scene moves down, camera moves up - Y decreases in Y-Down world)
            isPanning = true;
        }
        if (IsKeybindActive(m_controlSettings->GetKeybind(InputAction::PanLeft), io, false))
        {
            panInputAccumulator.x += effectivePanSpeed; // Negative X for local "left"
            isPanning = true;
        }
        if (IsKeybindActive(m_controlSettings->GetKeybind(InputAction::PanRight), io, false))
        {
            panInputAccumulator.x -= effectivePanSpeed; // Positive X for local "right"
            isPanning = true;
        }

        if (isPanning)
        {
            float currentRotationDegrees = m_camera->GetRotation();
            float currentRotationRadians = DegToRad(currentRotationDegrees);
            float cosAngle = std::cos(currentRotationRadians);
            float sinAngle = std::sin(currentRotationRadians);

            Vec2 rotatedPanDirection;
            rotatedPanDirection.x = panInputAccumulator.x * cosAngle - panInputAccumulator.y * sinAngle;
            rotatedPanDirection.y = panInputAccumulator.x * sinAngle + panInputAccumulator.y * cosAngle;

            m_camera->Pan(rotatedPanDirection);
        }

        // Zoom In/Out with keys
        float keyboardZoomFactor = 1.0f + (2.0f * io.DeltaTime);
        if (IsKeybindActive(m_controlSettings->GetKeybind(InputAction::ZoomIn), io, false))
        {
            m_camera->AdjustZoom(keyboardZoomFactor);
        }
        if (IsKeybindActive(m_controlSettings->GetKeybind(InputAction::ZoomOut), io, false))
        {
            m_camera->AdjustZoom(1.0f / keyboardZoomFactor);
        }

        // Rotation
        float deltaAngleDegrees = 0.0f;
        bool rotationKeyPressed = false;
        bool continuousRotation = m_controlSettings->m_freeRotation;

        KeyCombination rotateLeftKey = m_controlSettings->GetKeybind(InputAction::RotateLeft);
        KeyCombination rotateRightKey = m_controlSettings->GetKeybind(InputAction::RotateRight);

        if (IsKeybindActive(rotateLeftKey, io, !continuousRotation))
        {
            deltaAngleDegrees = continuousRotation ? freeRotationSpeedDegrees * io.DeltaTime : snapAngleDegrees;
            rotationKeyPressed = true;
        }
        if (IsKeybindActive(rotateRightKey, io, !continuousRotation))
        {
            deltaAngleDegrees = continuousRotation ? -freeRotationSpeedDegrees * io.DeltaTime : -snapAngleDegrees;
            rotationKeyPressed = true;
        }

        // This logic for combining inputs might need refinement if both keys are pressed for rotation.
        // Current assumes only one rotation key active at a time for deltaAngle assignment.

        if (rotationKeyPressed && deltaAngleDegrees != 0.0f)
        {
            float currentRotationDegrees = m_camera->GetRotation();
            float newRotationDegrees;

            if (continuousRotation)
            {
                newRotationDegrees = currentRotationDegrees + deltaAngleDegrees;
            }
            else
            {
                float intendedRotation = currentRotationDegrees + deltaAngleDegrees;
                newRotationDegrees = std::round(intendedRotation / snapAngleDegrees) * snapAngleDegrees;
                deltaAngleDegrees = newRotationDegrees - currentRotationDegrees;
            }

            // Pivot calculation always happens now
            Vec2 pivotWorld;
            bool canUseMousePivot = m_controlSettings->m_rotateAroundCursor && isViewportHovered;

            if (canUseMousePivot)
            {
                ImVec2 mousePosAbsolute = ImGui::GetMousePos();
                Vec2 mousePosInViewport = {mousePosAbsolute.x - viewportTopLeft.x,
                                           mousePosAbsolute.y - viewportTopLeft.y};
                // Check if mouse is within viewport bounds before using its position
                if (mousePosInViewport.x >= 0 && mousePosInViewport.x <= std::round(viewportSize.x) &&
                    mousePosInViewport.y >= 0 && mousePosInViewport.y <= std::round(viewportSize.y))
                {
                    pivotWorld = m_viewport->ScreenToWorld(mousePosInViewport, *m_camera);
                }
                else
                {
                    // Mouse outside viewport, fallback to viewport center for this rotation event
                    canUseMousePivot = false; // Force fallback to viewport center
                }
            }

            // If not using mouse pivot (either by setting or fallback), use viewport center
            if (!canUseMousePivot)
            {
                Vec2 viewportCenterScreen = {std::round(viewportSize.x) / 2.0f, std::round(viewportSize.y) / 2.0f};
                pivotWorld = m_viewport->ScreenToWorld(viewportCenterScreen, *m_camera);
            }

            Vec2 camPosWorld = m_camera->GetPosition();
            float actualDeltaAngleRadians = DegToRad(deltaAngleDegrees); // Use the potentially adjusted delta for snap
            float cosAngle = std::cos(actualDeltaAngleRadians);
            float sinAngle = std::sin(actualDeltaAngleRadians);

            Vec2 camRelativeToPivot = {camPosWorld.x - pivotWorld.x, camPosWorld.y - pivotWorld.y};
            Vec2 newCamRelativeToPivot;
            newCamRelativeToPivot.x = camRelativeToPivot.x * cosAngle - camRelativeToPivot.y * sinAngle;
            newCamRelativeToPivot.y = camRelativeToPivot.x * sinAngle + camRelativeToPivot.y * cosAngle;
            Vec2 newCamPosWorld = {newCamRelativeToPivot.x + pivotWorld.x,
                                   newCamRelativeToPivot.y + pivotWorld.y};

            m_camera->SetPosition(newCamPosWorld);
            m_camera->SetRotation(newRotationDegrees);
        }

        // Reset View (R key)
        if (IsKeybindActive(m_controlSettings->GetKeybind(InputAction::ResetView), io, true))
        {
            if (m_boardDataManager)
            {
                std::shared_ptr<Board> currentBoard = m_boardDataManager->getBoard();
                if (currentBoard && currentBoard->IsLoaded())
                {
                    BLRect boardBounds = currentBoard->GetBoundingBox(false); // Get bounds of visible layers
                    if (boardBounds.w > 0 || boardBounds.h > 0)
                    {                                                          // Check for valid bounds (width or height must be positive)
                        m_camera->FocusOnRect(boardBounds, *m_viewport, 0.1f); // 10% padding
                        // Optionally, reset rotation as well if desired for "Reset View"
                        // m_camera->SetRotation(0.0f);
                    }
                    else
                    {
                        // Board is loaded but has no visible extents (e.g., all layers off, or empty board)
                        m_camera->Reset(); // Fallback to default reset
                    }
                }
                else
                {
                    // No board loaded or board not marked as loaded
                    m_camera->Reset(); // Fallback to default reset
                }
            }
            else
            {
                // BoardDataManager not available
                m_camera->Reset(); // Fallback to default reset
            }
        }
    }
}