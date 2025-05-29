#include "ui/interaction/NavigationTool.hpp"
#include "view/Camera.hpp"
#include "view/Viewport.hpp"
#include "core/ControlSettings.hpp"
#include "core/InputActions.hpp"     // Required for InputAction and KeyCombination
#include "core/BoardDataManager.hpp" // Added include
#include "pcb/Board.hpp"             // Added include for BLRect and Board methods
#include "imgui.h"                   // For ImGui:: functions
#include <cmath>                     // For M_PI if not already included by camera/viewport
#include <algorithm>                 // for std::max if not already there
#include <iostream>                  // for std::cout and std::endl
#include "pcb/elements/Trace.hpp"
#include "pcb/elements/Pin.hpp"
#include "pcb/elements/Via.hpp"
#include "pcb/elements/Arc.hpp"
#include "pcb/elements/TextLabel.hpp"
#include "pcb/elements/Component.hpp"

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
    : InteractionTool("Navigation", camera, viewport),
      m_controlSettings(controlSettings),
      m_boardDataManager(boardDataManager),
      m_isHoveringElement(false), // Initialize new members
      m_selectedNetId(-1)
{
}

void NavigationTool::ProcessInput(ImGuiIO &io, bool isViewportFocused, bool isViewportHovered, ImVec2 viewportTopLeft, ImVec2 viewportSize)
{
    if (!m_camera || !m_viewport || !m_controlSettings || !m_boardDataManager)
        return;

    // Update viewport with current dimensions from the window
    m_viewport->SetDimensions(0, 0, static_cast<int>(std::round(viewportSize.x)), static_cast<int>(std::round(viewportSize.y)));

    // --- Hover and Selection Logic ---
    std::shared_ptr<const Board> currentBoard = m_boardDataManager->getBoard();
    bool boardAvailable = currentBoard && currentBoard->IsLoaded();
    // Reset hover state for this frame - moved up so it's always reset
    m_isHoveringElement = false;
    m_hoveredElementInfo = "";

    if (isViewportHovered && boardAvailable)
    {
        ImVec2 screenMousePos = io.MousePos;
        ImVec2 viewportMousePos_Im = ImVec2(screenMousePos.x - viewportTopLeft.x, screenMousePos.y - viewportTopLeft.y);

        if (viewportMousePos_Im.x >= 0 && viewportMousePos_Im.x <= viewportSize.x &&
            viewportMousePos_Im.y >= 0 && viewportMousePos_Im.y <= viewportSize.y)
        {
            Vec2 viewportMousePos_Vec2 = {viewportMousePos_Im.x, viewportMousePos_Im.y};
            Vec2 worldMousePos = m_viewport->ScreenToWorld(viewportMousePos_Vec2, *m_camera);

            float pickTolerance = 2.0f / m_camera->GetZoom(); // World units
            pickTolerance = std::max(0.01f, pickTolerance);   // Ensure minimum pick tolerance

            // Get all potentially interactive elements
            std::vector<ElementInteractionInfo> interactiveElements = currentBoard->GetAllVisibleElementsForInteraction();

            // Check for hover
            // Iterate in reverse to prioritize elements rendered on top (assuming later elements in vector are "on top")
            // However, GetAllVisibleElementsForInteraction doesn't guarantee render order.
            // For now, simple iteration. If z-ordering becomes an issue, this needs refinement.
            for (const auto &item : interactiveElements)
            {
                if (!item.element)
                {
                    continue;
                }
                if (item.element->isHit(worldMousePos, pickTolerance, item.parentComponent))
                {
                    m_isHoveringElement = true;
                    m_hoveredElementInfo = item.element->getInfo(item.parentComponent);
                    break; // Found a hovered element
                }
            }

            // Handle Mouse Click for Selection (Left Click)
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && isViewportFocused)
            {
                int clickedNetId = -1;
                for (const auto &item : interactiveElements)
                {
                    if (item.element && item.element->isHit(worldMousePos, pickTolerance, item.parentComponent))
                    {
                        clickedNetId = item.element->getNetId(); // Assuming getNetId() is part of Element base or handled by derived.
                                                                 // If element is not associated with a net, it should return -1 or similar.
                        if (clickedNetId != -1)
                            break; // Found an element with a net
                    }
                }

                if (m_boardDataManager->GetSelectedNetId() == clickedNetId && clickedNetId != -1)
                { // Clicking an already selected net deselects it
                    m_boardDataManager->SetSelectedNetId(-1);
                    std::cout << "NavigationTool: Deselected Net ID: " << clickedNetId << std::endl;
                }
                else if (clickedNetId != -1)
                {
                    m_boardDataManager->SetSelectedNetId(clickedNetId);
                    std::cout << "NavigationTool: Selected Net ID: " << clickedNetId << std::endl;
                }
                else
                {
                    m_boardDataManager->SetSelectedNetId(-1); // Clicked on empty space or non-net element
                    std::cout << "NavigationTool: Clicked empty or non-net element, selection cleared." << std::endl;
                }
                // mark the board as dirty pcbrenderer
            }
        }
        // If mouse is outside viewport content area, m_isHoveringElement remains false (or was set false at the start)
    }
    // If not hovering viewport or no board, m_isHoveringElement remains false (or was set false at the start)

    // --- End Hover and Selection Logic ---

    // Existing Navigation Logic (Zooming, Panning, Keyboard controls)
    // Only process navigation if viewport is hovered or the window has focus (for keyboard input)
    if (!isViewportHovered && !isViewportFocused)
    {
        // If only hover/selection logic was processed and no focus/hover for navigation, display tooltip and exit.
        if (m_isHoveringElement && !m_hoveredElementInfo.empty() && boardAvailable)
        {
            std::cout << "NavigationTool: Displaying tooltip for hovered element: " << m_hoveredElementInfo << std::endl;
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
            ImGui::TextUnformatted(m_hoveredElementInfo.c_str());
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
        return;
    }

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
            panInputAccumulator.y += effectivePanSpeed; // Positive Y for local "up"
            isPanning = true;
        }
        if (IsKeybindActive(m_controlSettings->GetKeybind(InputAction::PanDown), io, false))
        {
            panInputAccumulator.y -= effectivePanSpeed; // Negative Y for local "down"
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
                std::shared_ptr<const Board> currentBoard = m_boardDataManager->getBoard();
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

    // Display Tooltip (should be after all input processing for the frame for this tool)
    if (m_isHoveringElement && !m_hoveredElementInfo.empty() && boardAvailable && isViewportHovered) // Check isViewportHovered again for safety
    {
        ImGui::SetNextWindowSize(ImVec2(300, 0)); // Example: Set a max width for the tooltip, height automatic
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(m_hoveredElementInfo.c_str());
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

void NavigationTool::OnActivated()
{
    std::cout << GetName() << " activated." << std::endl;
    // Reset selection or hover state if desired when tool becomes active
    m_boardDataManager->SetSelectedNetId(-1);
    m_isHoveringElement = false;
    m_hoveredElementInfo = "";
}

void NavigationTool::OnDeactivated()
{
    std::cout << GetName() << " deactivated." << std::endl;
    // Clear hover state when tool is deactivated
    m_isHoveringElement = false;
    m_hoveredElementInfo = "";
    // Optionally keep m_selectedNetId or clear it based on desired behavior
}

int NavigationTool::GetSelectedNetId() const
{
    return m_boardDataManager->GetSelectedNetId();
}

void NavigationTool::ClearSelection()
{
    m_boardDataManager->SetSelectedNetId(-1);
    std::cout << "NavigationTool: Selection cleared." << std::endl;
    // Potentially trigger a highlight update to clear previous highlighting if PcbRenderer is accessible
}

// The old helper methods (CheckElementHover, GetNetIdAtPosition, IsMouseOverTrace, etc.)
// are now removed as their logic is handled by Element::isHit and Element::getInfo/getNetId
// and the loop over GetAllVisibleElementsForInteraction.}