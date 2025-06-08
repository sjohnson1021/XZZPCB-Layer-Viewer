#include "ui/interaction/NavigationTool.hpp"

#include <iostream>

#include "imgui.h"  // For ImGui:: functions

#include "core/BoardDataManager.hpp"  // Added include
#include "core/ControlSettings.hpp"
#include "core/InputActions.hpp"  // Required for InputAction and KeyCombination
#include "pcb/Board.hpp"          // Added include for BLRect and Board methods
#include "pcb/elements/Arc.hpp"
#include "pcb/elements/Component.hpp"
#include "pcb/elements/Pin.hpp"
#include "pcb/elements/TextLabel.hpp"
#include "pcb/elements/Trace.hpp"
#include "pcb/elements/Via.hpp"
#include "view/Camera.hpp"
#include "view/Viewport.hpp"

// Define PI if not available
#ifndef M_PI
#    define M_PI 3.14159265358979323846
#endif

inline static float DegToRad(float degrees)
{
    return degrees * (static_cast<float>(M_PI) / 180.0F);
}

// Helper function to check if a keybind is active
static bool IsKeybindActive(const KeyCombination& kb, ImGuiIO& io, bool use_is_key_pressed)
{
    if (!kb.IsBound()) {
        return false;
    }

    bool key_state_correct = false;
    if (use_is_key_pressed) {
        key_state_correct = ImGui::IsKeyPressed(kb.key, false);
    } else {
        key_state_correct = ImGui::IsKeyDown(kb.key);
    }
    if (!key_state_correct) {
        return false;
    }

    // Check modifiers
    if (kb.ctrl && !io.KeyCtrl) {
        return false;
    }
    if (kb.shift && !io.KeyShift) {
        return false;
    }
    if (kb.alt && !io.KeyAlt) {
        return false;
    }

    // If we require exact modifiers (e.g. Ctrl must be down, others must be up)
    // if (!kb.ctrl && io.KeyCtrl) return false; // etc.
    // For now, we just check if required modifiers are pressed.

    return true;
}

NavigationTool::NavigationTool(std::shared_ptr<Camera> camera,
                               std::shared_ptr<Viewport> viewport,
                               std::shared_ptr<ControlSettings> control_settings,
                               std::shared_ptr<BoardDataManager> board_data_manager)
    : InteractionTool("Navigation", camera, viewport),
      m_control_settings_(control_settings),
      m_board_data_manager_(board_data_manager),
      m_is_hovering_element_(false),  // Initialize new members
      m_selected_net_id_(-1)
{
}

void NavigationTool::ProcessInput(ImGuiIO& io, bool is_viewport_focused, bool is_viewport_hovered, ImVec2 viewport_top_left, ImVec2 viewport_size)
{
    const std::shared_ptr<Camera> camera = GetCamera();
    const std::shared_ptr<Viewport> viewport = GetViewport();
    if (!camera || !viewport || !m_control_settings_ || !m_board_data_manager_)
        return;

    // Update viewport with current dimensions from the window
    viewport->SetDimensions(0, 0, static_cast<int>(std::round(viewport_size.x)), static_cast<int>(std::round(viewport_size.y)));

    // --- Hover and Selection Logic ---
    std::shared_ptr<const Board> current_board = m_board_data_manager_->GetBoard();
    bool board_available = current_board && current_board->IsLoaded();
    // Reset hover state for this frame - moved up so it's always reset
    m_is_hovering_element_ = false;
    m_hovered_element_info_ = "";

    if (is_viewport_hovered && board_available) {
        ImVec2 screenMousePos = io.MousePos;
        ImVec2 viewportMousePos_Im = ImVec2(screenMousePos.x - viewport_top_left.x, screenMousePos.y - viewport_top_left.y);

        if (viewportMousePos_Im.x >= 0 && viewportMousePos_Im.x <= viewport_size.x && viewportMousePos_Im.y >= 0 && viewportMousePos_Im.y <= viewport_size.y) {
            Vec2 viewportMousePos_Vec2 = {viewportMousePos_Im.x, viewportMousePos_Im.y};
            Vec2 worldMousePos = viewport->ScreenToWorld(viewportMousePos_Vec2, *camera);

            // No coordinate transformation needed since actual element coordinates are now updated
            // when board flip state changes, ensuring hitbox detection is always synchronized
            Vec2 transformedWorldMousePos = worldMousePos;

            float pick_tolerance = 2.0f / camera->GetZoom();   // World units
            pick_tolerance = std::max(0.01f, pick_tolerance);  // Ensure minimum pick tolerance

            // Get all potentially interactive elements
            std::vector<ElementInteractionInfo> interactive_elements = current_board->GetAllVisibleElementsForInteraction();

            // Check for hover
            // Iterate in reverse to prioritize elements rendered on top (assuming later elements in vector are "on top")
            // However, GetAllVisibleElementsForInteraction doesn't guarantee render order.
            // For now, simple iteration. If z-ordering becomes an issue, this needs refinement.
            for (const auto& item : interactive_elements) {
                if (!item.element) {
                    continue;
                }
                if (item.element->IsHit(transformedWorldMousePos, pick_tolerance, item.parent_component)) {
                    m_is_hovering_element_ = true;
                    m_hovered_element_info_ = item.element->GetInfo(item.parent_component, current_board.get());
                    break;  // Found a hovered element
                }
            }

            // Handle Mouse Click for Selection (Left Click)
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && is_viewport_focused) {
                int clicked_net_id = -1;
                for (const auto& item : interactive_elements) {
                    if (item.element && item.element->IsHit(transformedWorldMousePos, pick_tolerance, item.parent_component)) {
                        clicked_net_id = item.element->GetNetId();  // Assuming getNetId() is part of Element base or handled by derived.
                                                                    // If element is not associated with a net, it should return -1 or similar.
                        if (clicked_net_id != -1)
                            break;  // Found an element with a net
                    }
                }

                if (m_board_data_manager_->GetSelectedNetId() == clicked_net_id && clicked_net_id != -1) {  // Clicking an already selected net deselects it
                    m_board_data_manager_->SetSelectedNetId(-1);
                    std::cout << "NavigationTool: Deselected Net ID: " << clicked_net_id << std::endl;
                } else if (clicked_net_id != -1) {
                    m_board_data_manager_->SetSelectedNetId(clicked_net_id);
                    std::cout << "NavigationTool: Selected Net ID: " << clicked_net_id << std::endl;
                } else {
                    m_board_data_manager_->SetSelectedNetId(-1);  // Clicked on empty space or non-net element
                    std::cout << "NavigationTool: Clicked empty or non-net element, selection cleared." << std::endl;
                }
                // mark the board as dirty pcbrenderer
            }

            // Handle Middle Mouse Click for Board Side Toggle
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Middle) && is_viewport_focused) {
                // CRITICAL FIX: Check if board flipping is allowed before attempting to flip
                if (m_board_data_manager_->CanFlipBoard()) {
                    m_board_data_manager_->ToggleViewSide();
                    std::cout << "NavigationTool: Board view toggled to " << BoardSideToString(m_board_data_manager_->GetCurrentViewSide()) << std::endl;
                } else {
                    std::cout << "NavigationTool: Board flipping disabled - folding must be enabled and viewing Top/Bottom side" << std::endl;
                }
            }
        }
        // If mouse is outside viewport content area, m_isHoveringElement remains false (or was set false at the start)
    }
    // If not hovering viewport or no board, m_isHoveringElement remains false (or was set false at the start)

    // --- End Hover and Selection Logic ---

    // Existing Navigation Logic (Zooming, Panning, Keyboard controls)
    // Only process navigation if viewport is hovered or the window has focus (for keyboard input)
    if (!is_viewport_hovered && !is_viewport_focused) {
        // If only hover/selection logic was processed and no focus/hover for navigation, display tooltip and exit.
        if (m_is_hovering_element_ && !m_hovered_element_info_.empty() && board_available) {
            std::cout << "NavigationTool: Displaying tooltip for hovered element: " << m_hovered_element_info_ << std::endl;
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
            ImGui::TextUnformatted(m_hovered_element_info_.c_str());
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
        return;
    }

    // Zooming with mouse wheel (only if hovered over the content area)
    if (io.MouseWheel != 0.0f && is_viewport_hovered) {
        float zoom_sensitivity = 1.1f;
        float zoom_factor = (io.MouseWheel > 0.0f) ? zoom_sensitivity : 1.0f / zoom_sensitivity;

        ImVec2 mousePosAbsolute = ImGui::GetMousePos();
        // viewportTopLeft is the screen coordinate of the top-left of our render area (ImGui::Image)
        Vec2 mouse_pos_in_viewport = {mousePosAbsolute.x - viewport_top_left.x, mousePosAbsolute.y - viewport_top_left.y};

        Vec2 world_pos_under_mouse = viewport->ScreenToWorld(mouse_pos_in_viewport, *camera);
        float old_zoom_val = camera->GetZoom();
        camera->SetZoom(old_zoom_val * zoom_factor);
        float new_zoom_val = camera->GetZoom();
        Vec2 cam_pos_val = camera->GetPosition();
        if (new_zoom_val != 0.0f && old_zoom_val != 0.0f) {
            Vec2 new_pos = cam_pos_val + (world_pos_under_mouse - cam_pos_val) * (1.0f - old_zoom_val / new_zoom_val);
            camera->SetPosition(new_pos);
            // debug print new_pos
            std::cout << "newPos: " << new_pos.x_ax << ", " << new_pos.y_ax << std::endl;
        }
    }

    // Panning (Right Mouse Button + Drag only, middle mouse is now used for board side toggle)
    if (is_viewport_hovered && ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
        ImVec2 delta = io.MouseDelta;
        if (delta.x != 0.0f || delta.y != 0.0f) {
            Vec2 world_delta = viewport->ScreenDeltaToWorldDelta({delta.x, delta.y}, *camera);
            camera->Pan(world_delta);
        }
    }
    bool is_panning = false;

    // Keyboard controls (active if window is focused)
    if (is_viewport_focused) {
        float pan_speed = 100.0f / camera->GetZoom();                           // World units per second
        pan_speed = std::max(1.0f, pan_speed);                                  // Ensure a minimum pan speed at high zoom
        float free_rotation_speed_degrees = 90.0f;                              // Degrees per second (for free rotation)
        float snap_angle_degrees = m_control_settings_->m_snap_rotation_angle;  // Degrees for snap rotation

        // Pan controls (now relative to camera rotation)
        Vec2 pan_input_accumulator = {0.0f, 0.0f};
        float effective_pan_speed = pan_speed * io.DeltaTime;

        if (IsKeybindActive(m_control_settings_->GetKeybind(InputAction::kPanUp), io, false)) {
            pan_input_accumulator.y_ax += effective_pan_speed;  // Positive Y for local "up"
            is_panning = true;
        }
        if (IsKeybindActive(m_control_settings_->GetKeybind(InputAction::kPanDown), io, false)) {
            pan_input_accumulator.y_ax -= effective_pan_speed;  // Negative Y for local "down"
            is_panning = true;
        }
        if (IsKeybindActive(m_control_settings_->GetKeybind(InputAction::kPanLeft), io, false)) {
            pan_input_accumulator.x_ax += effective_pan_speed;  // Negative X for local "left"
            is_panning = true;
        }
        if (IsKeybindActive(m_control_settings_->GetKeybind(InputAction::kPanRight), io, false))
            pan_input_accumulator.x_ax -= effective_pan_speed;  // Positive X for local "right"
        is_panning = true;

        if (is_panning) {
            float current_rotation_degrees = camera->GetRotation();
            float current_rotation_radians = DegToRad(current_rotation_degrees);
            float cos_angle = std::cos(current_rotation_radians);
            float sin_angle = std::sin(current_rotation_radians);

            Vec2 rotated_pan_direction;
            rotated_pan_direction.x_ax = pan_input_accumulator.x_ax * cos_angle - pan_input_accumulator.y_ax * sin_angle;
            rotated_pan_direction.y_ax = pan_input_accumulator.x_ax * sin_angle + pan_input_accumulator.y_ax * cos_angle;

            camera->Pan(rotated_pan_direction);
        }

        // Zoom In/Out with keys
        float keyboard_zoom_factor = 1.0f + (2.0f * io.DeltaTime);
        if (IsKeybindActive(m_control_settings_->GetKeybind(InputAction::kZoomIn), io, false)) {
            camera->AdjustZoom(keyboard_zoom_factor);
        }
        if (IsKeybindActive(m_control_settings_->GetKeybind(InputAction::kZoomOut), io, false)) {
            camera->AdjustZoom(1.0f / keyboard_zoom_factor);
        }

        // Rotation
        float delta_angle_degrees = 0.0f;
        bool rotation_key_pressed = false;
        bool continuous_rotation = m_control_settings_->m_free_rotation;

        KeyCombination rotate_left_key = m_control_settings_->GetKeybind(InputAction::kRotateLeft);
        KeyCombination rotate_right_key = m_control_settings_->GetKeybind(InputAction::kRotateRight);

        if (IsKeybindActive(rotate_left_key, io, !continuous_rotation)) {
            delta_angle_degrees = continuous_rotation ? free_rotation_speed_degrees * io.DeltaTime : snap_angle_degrees;
            rotation_key_pressed = true;
        }
        if (IsKeybindActive(rotate_right_key, io, !continuous_rotation)) {
            delta_angle_degrees = continuous_rotation ? -free_rotation_speed_degrees * io.DeltaTime : -snap_angle_degrees;
            rotation_key_pressed = true;
        }

        // This logic for combining inputs might need refinement if both keys are pressed for rotation.
        // Current assumes only one rotation key active at a time for deltaAngle assignment.

        if (rotation_key_pressed && delta_angle_degrees != 0.0f) {
            float current_rotation_degrees = camera->GetRotation();
            float new_rotation_degrees;

            if (continuous_rotation) {
                new_rotation_degrees = current_rotation_degrees + delta_angle_degrees;
            } else {
                float intended_rotation = current_rotation_degrees + delta_angle_degrees;
                new_rotation_degrees = std::round(intended_rotation / snap_angle_degrees) * snap_angle_degrees;
                delta_angle_degrees = new_rotation_degrees - current_rotation_degrees;
            }

            // Pivot calculation always happens now
            Vec2 pivot_world;
            bool canUseMousePivot = m_control_settings_->m_rotate_around_cursor && is_viewport_hovered;

            if (canUseMousePivot) {
                ImVec2 mousePosAbsolute = ImGui::GetMousePos();
                Vec2 mouse_pos_in_viewport = {mousePosAbsolute.x - viewport_top_left.x, mousePosAbsolute.y - viewport_top_left.y};
                // Check if mouse is within viewport bounds before using its position
                if (mouse_pos_in_viewport.x_ax >= 0 && mouse_pos_in_viewport.x_ax <= std::round(viewport_size.x) && mouse_pos_in_viewport.y_ax >= 0 &&
                    mouse_pos_in_viewport.y_ax <= std::round(viewport_size.y)) {
                    pivot_world = viewport->ScreenToWorld(mouse_pos_in_viewport, *camera);
                } else {
                    // Mouse outside viewport, fallback to viewport center for this rotation event
                    canUseMousePivot = false;  // Force fallback to viewport center
                }
            }

            // If not using mouse pivot (either by setting or fallback), use viewport center
            if (!canUseMousePivot) {
                Vec2 viewport_center_screen = {std::round(viewport_size.x) / 2.0f, std::round(viewport_size.y) / 2.0f};
                pivot_world = viewport->ScreenToWorld(viewport_center_screen, *camera);
            }

            Vec2 cam_pos_world = camera->GetPosition();
            float actual_delta_angle_radians = DegToRad(delta_angle_degrees);  // Use the potentially adjusted delta for snap
            float cos_angle = std::cos(actual_delta_angle_radians);
            float sin_angle = std::sin(actual_delta_angle_radians);

            Vec2 cam_relative_to_pivot = {cam_pos_world.x_ax - pivot_world.x_ax, cam_pos_world.y_ax - pivot_world.y_ax};
            Vec2 new_cam_relative_to_pivot;
            new_cam_relative_to_pivot.x_ax = cam_relative_to_pivot.x_ax * cos_angle - cam_relative_to_pivot.y_ax * sin_angle;
            new_cam_relative_to_pivot.y_ax = cam_relative_to_pivot.x_ax * sin_angle + cam_relative_to_pivot.y_ax * cos_angle;
            Vec2 new_cam_pos_world = {new_cam_relative_to_pivot.x_ax + pivot_world.x_ax, new_cam_relative_to_pivot.y_ax + pivot_world.y_ax};

            camera->SetPosition(new_cam_pos_world);
            camera->SetRotation(new_rotation_degrees);
        }

        // Reset View (R key)
        if (IsKeybindActive(m_control_settings_->GetKeybind(InputAction::kResetView), io, true)) {
            if (m_board_data_manager_) {
                std::shared_ptr<const Board> current_board = m_board_data_manager_->GetBoard();
                if (current_board && current_board->IsLoaded()) {
                    BLRect board_bounds = current_board->GetBoundingBox(false);  // Get bounds of visible layers
                    if (board_bounds.w > 0 || board_bounds.h > 0) {              // Check for valid bounds (width or height must be positive)
                        camera->FocusOnRect(board_bounds, *viewport, 0.1f);      // 10% padding
                        // Optionally, reset rotation as well if desired for "Reset View"
                        // m_camera->SetRotation(0.0f);
                    } else {
                        // Board is loaded but has no visible extents (e.g., all layers off, or empty board)
                        camera->Reset();  // Fallback to default reset
                    }
                } else {
                    // No board loaded or board not marked as loaded
                    camera->Reset();  // Fallback to default reset
                }
            } else {
                // BoardDataManager not available
                camera->Reset();  // Fallback to default reset
            }
        }

        // Flip Board (F key) - unified board flip behavior
        if (IsKeybindActive(m_control_settings_->GetKeybind(InputAction::kFlipBoard), io, true)) {
            if (m_board_data_manager_) {
                // CRITICAL FIX: Check if board flipping is allowed before attempting to flip
                if (m_board_data_manager_->CanFlipBoard()) {
                    // Use ToggleViewSide which now handles both side switching and mirroring
                    m_board_data_manager_->ToggleViewSide();
                    std::cout << "NavigationTool: Board flipped to " << BoardSideToString(m_board_data_manager_->GetCurrentViewSide()) << std::endl;
                } else {
                    std::cout << "NavigationTool: Board flipping disabled - folding must be enabled and viewing Top/Bottom side" << std::endl;
                }
            }
        }
    }

    // Display Tooltip (should be after all input processing for the frame for this tool)
    if (m_is_hovering_element_ && !m_hovered_element_info_.empty() && board_available && is_viewport_hovered)  // Check isViewportHovered again for safety
    {
        ImGui::SetNextWindowSize(ImVec2(300, 0));  // Example: Set a max width for the tooltip, height automatic
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(m_hovered_element_info_.c_str());
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

void NavigationTool::OnActivated()
{
    std::cout << GetName() << " activated." << std::endl;
    // Reset selection or hover state if desired when tool becomes active
    m_board_data_manager_->SetSelectedNetId(-1);
    m_is_hovering_element_ = false;
    m_hovered_element_info_ = "";
}

void NavigationTool::OnDeactivated()
{
    std::cout << GetName() << " deactivated." << std::endl;
    // Clear hover state when tool is deactivated
    m_is_hovering_element_ = false;
    m_hovered_element_info_ = "";
    // Optionally keep m_selectedNetId or clear it based on desired behavior
}

int NavigationTool::GetSelectedNetId() const
{
    return m_board_data_manager_->GetSelectedNetId();
}

void NavigationTool::ClearSelection()
{
    m_board_data_manager_->SetSelectedNetId(-1);
    std::cout << "NavigationTool: Selection cleared." << std::endl;
    // Potentially trigger a highlight update to clear previous highlighting if PcbRenderer is accessible
}

// TransformMousePositionForBoardFlip method removed - no longer needed since
// actual element coordinates are updated when board flip state changes

// The old helper methods (CheckElementHover, GetNetIdAtPosition, IsMouseOverTrace, etc.)
// are now removed as their logic is handled by Element::isHit and Element::getInfo/getNetId
// and the loop over GetAllVisibleElementsForInteraction.
