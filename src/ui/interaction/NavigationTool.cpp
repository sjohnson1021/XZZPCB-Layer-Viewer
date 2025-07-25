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
      m_selected_net_id_(-1),
      m_cache_valid_(false),  // Initialize cache as invalid
      m_cached_board_side_(BoardDataManager::BoardSide::kBoth)
{
    // Note: We don't register callbacks to avoid interfering with existing callbacks
    // from PcbRenderer and Application. Instead, we use a polling approach to detect changes.
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

            // Get cached interactive elements (performance optimization)
            const std::vector<ElementInteractionInfo>& interactive_elements = GetCachedInteractiveElements();

            // Check for hover
            // Iterate in reverse to prioritize elements rendered on top (assuming later elements in vector are "on top")
            // However, GetCachedInteractiveElements doesn't guarantee render order.
            // OPTIMIZATION: Use spatial indexing for faster hit detection
            // This replaces the expensive linear search that was identified as a bottleneck in VTune
            const Element* hit_element = nullptr;
            const Component* hit_parent_component = nullptr;

            // Try optimized spatial hit detection first
            if (m_render_pipeline_ && m_render_pipeline_->IsInitialized()) {
                hit_element = m_render_pipeline_->FindHitElementOptimized(transformedWorldMousePos, pick_tolerance, nullptr);

                // If spatial index found an element, find its parent component from cache
                if (hit_element) {
                    for (const auto& item : interactive_elements) {
                        if (item.element == hit_element) {
                            hit_parent_component = item.parent_component;
                            break;
                        }
                    }
                }
            }

            // Fallback to linear search if spatial index is not available or failed
            if (!hit_element) {
                for (const auto& item : interactive_elements) {
                    if (!item.element) {
                        continue;
                    }
                    if (item.element->IsHit(transformedWorldMousePos, pick_tolerance, item.parent_component)) {
                        hit_element = item.element;
                        hit_parent_component = item.parent_component;
                        break;  // Found a hovered element
                    }
                }
            }

            // Update hover state
            if (hit_element) {
                m_is_hovering_element_ = true;
                m_hovered_element_info_ = hit_element->GetInfo(hit_parent_component, current_board.get());
            }

            // Handle Mouse Click for Selection (Left Click)
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && is_viewport_focused) {
                int clicked_net_id = -1;
                const Element* clicked_element = nullptr;

                // OPTIMIZATION: Reuse hit detection result from hover if mouse hasn't moved significantly
                static Vec2 last_mouse_pos = {-1000.0f, -1000.0f};
                static const Element* last_hit_element = nullptr;

                float mouse_move_distance = geometry_utils::FastDistance(transformedWorldMousePos, last_mouse_pos);

                if (mouse_move_distance < pick_tolerance && last_hit_element == hit_element) {
                    // Reuse previous hit detection result
                    clicked_element = hit_element;
                    if (clicked_element) {
                        clicked_net_id = clicked_element->GetNetId();
                    }
                } else {
                    // Perform fresh hit detection using optimized method
                    if (m_render_pipeline_ && m_render_pipeline_->IsInitialized()) {
                        clicked_element = m_render_pipeline_->FindHitElementOptimized(transformedWorldMousePos, pick_tolerance, nullptr);
                    }

                    // Fallback to linear search if needed
                    if (!clicked_element) {
                        for (const auto& item : interactive_elements) {
                            if (item.element && item.element->IsHit(transformedWorldMousePos, pick_tolerance, item.parent_component)) {
                                clicked_element = item.element;
                                break;  // Found the first hit element (prioritize by render order)
                            }
                        }
                    }

                    if (clicked_element) {
                        clicked_net_id = clicked_element->GetNetId();
                    }

                    // Update cache
                    last_mouse_pos = transformedWorldMousePos;
                    last_hit_element = clicked_element;
                }

                // Always set the selected element (even if nullptr for empty space)
                m_board_data_manager_->SetSelectedElement(clicked_element);

				if (clicked_net_id != -1) {
                    m_board_data_manager_->SetSelectedNetId(clicked_net_id);
                    #ifdef DEBUG_NAVIGATION
                    std::cout << "NavigationTool: Selected Net ID: " << clicked_net_id << std::endl;
                    #endif
                } else {
                    m_board_data_manager_->SetSelectedNetId(-1);  // Clicked on empty space or non-net element
                    #ifdef DEBUG_NAVIGATION
                    std::cout << "NavigationTool: Clicked empty or non-net element, selection cleared." << std::endl;
                    #endif
                }

                #ifdef DEBUG_NAVIGATION
                if (clicked_element) {
                    std::cout << "NavigationTool: Selected Element: " << clicked_element->GetInfo() << std::endl;
                } else {
                    std::cout << "NavigationTool: No element selected." << std::endl;
                }
                #endif
                // mark the board as dirty pcbrenderer
            }

            // Handle Middle Mouse Click for Board Side Toggle
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Middle) && is_viewport_focused) {
                // CRITICAL FIX: Check if board flipping is allowed before attempting to flip
                if (m_board_data_manager_->CanFlipBoard()) {
                    m_board_data_manager_->ToggleViewSide();
                    // Cache will be automatically invalidated on next access due to board side change
                    #ifdef DEBUG_NAVIGATION
                    std::cout << "NavigationTool: Board view toggled to " << BoardSideToString(m_board_data_manager_->GetCurrentViewSide()) << std::endl;
                    #endif
                } else {
                    #ifdef DEBUG_NAVIGATION
                    std::cout << "NavigationTool: Board flipping disabled - folding must be enabled and viewing Top/Bottom side" << std::endl;
                    #endif
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
        float zoom_sensitivity = m_control_settings_->m_zoom_sensitivity;
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
        float base_pan_speed = 100.0f / camera->GetZoom();                      // World units per second
        base_pan_speed = std::max(1.0f, base_pan_speed);                        // Ensure a minimum pan speed at high zoom
        float pan_speed = base_pan_speed * m_control_settings_->m_pan_speed_multiplier;  // Apply user multiplier
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
        float keyboard_zoom_base = 1.0f + (2.0f * io.DeltaTime);
        float keyboard_zoom_factor = 1.0f + ((keyboard_zoom_base - 1.0f) * m_control_settings_->m_zoom_sensitivity / 1.1f);
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

    // Invalidate cache when tool is activated to ensure fresh data
    InvalidateElementCache();
}

void NavigationTool::OnDeactivated()
{
    std::cout << GetName() << " deactivated." << std::endl;
    // Clear hover state when tool is deactivated
    m_is_hovering_element_ = false;
    m_hovered_element_info_ = "";
    // Optionally keep m_selectedNetId or clear it based on desired behavior

    // Clear cache when tool is deactivated to free memory
    InvalidateElementCache();
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
// and the loop over GetCachedInteractiveElements.

// --- Performance Optimization: Element Caching System Implementation ---

void NavigationTool::InvalidateElementCache() const
{
    m_cache_valid_ = false;
    m_cached_interactive_elements_.clear();
    m_cached_layer_visibility_.clear();
    m_cached_board_.reset();
}

bool NavigationTool::HasCacheInvalidatingChanges() const
{
    if (!m_board_data_manager_) {
        return true;  // No board data manager means cache should be invalid
    }

    std::shared_ptr<const Board> current_board = m_board_data_manager_->GetBoard();

    // Check if board changed (most common case)
    if (current_board != m_cached_board_) {
        return true;
    }

    // Check if board side changed (second most common case)
    if (m_board_data_manager_->GetCurrentViewSide() != m_cached_board_side_) {
        return true;
    }

    // Only check layer visibility if we have a valid board and cached state
    if (current_board && current_board->IsLoaded() && !m_cached_layer_visibility_.empty()) {
        int layer_count = current_board->GetLayerCount();
        if (m_cached_layer_visibility_.size() != static_cast<size_t>(layer_count)) {
            return true;
        }

        // Quick check: only compare a few key layers first (optimization for large layer counts)
        // Check layers 0, 1, 16, 28 (common important layers) before doing full scan
        const std::vector<int> key_layers = {0, 1, 16, 28};
        for (int layer_id : key_layers) {
            if (layer_id < layer_count) {
                if (m_board_data_manager_->IsLayerVisible(layer_id) != m_cached_layer_visibility_[layer_id]) {
                    return true;
                }
            }
        }

        // If key layers match, do full scan (less frequent)
        for (int i = 0; i < layer_count; ++i) {
            if (m_board_data_manager_->IsLayerVisible(i) != m_cached_layer_visibility_[i]) {
                return true;
            }
        }
    }

    return false;
}

void NavigationTool::UpdateElementCache() const
{
    if (!m_board_data_manager_) {
        InvalidateElementCache();
        return;
    }

    std::shared_ptr<const Board> current_board = m_board_data_manager_->GetBoard();
    if (!current_board || !current_board->IsLoaded()) {
        InvalidateElementCache();
        return;
    }

    // Update cached state
    m_cached_board_ = current_board;
    m_cached_board_side_ = m_board_data_manager_->GetCurrentViewSide();

    int layer_count = current_board->GetLayerCount();
    m_cached_layer_visibility_.resize(layer_count);
    for (int i = 0; i < layer_count; ++i) {
        m_cached_layer_visibility_[i] = m_board_data_manager_->IsLayerVisible(i);
    }

    // Get fresh elements from the board
    m_cached_interactive_elements_ = current_board->GetAllVisibleElementsForInteraction();
    m_cache_valid_ = true;

    // Only log cache updates for large boards to avoid spam
    if (m_cached_interactive_elements_.size() > 1000) {
        std::cout << "NavigationTool: Element cache updated with " << m_cached_interactive_elements_.size() << " elements" << std::endl;
    }
}

const std::vector<ElementInteractionInfo>& NavigationTool::GetCachedInteractiveElements() const
{
    // Check if cache needs to be invalidated due to changes
    if (m_cache_valid_ && HasCacheInvalidatingChanges()) {
        // Only log cache invalidation for large boards to avoid spam
        if (m_cached_interactive_elements_.size() > 1000) {
            std::cout << "NavigationTool: Cache invalidated due to detected changes" << std::endl;
        }
        m_cache_valid_ = false;
    }

    if (!m_cache_valid_) {
        UpdateElementCache();
    }
    return m_cached_interactive_elements_;
}

// Spatial hit detection with memory optimization
const Element* NavigationTool::FindHitElementOptimized(const Vec2& world_pos, float tolerance) {
    // Use cached results if available and valid
    static Vec2 last_query_pos = {-1000.0f, -1000.0f};
    static const Element* last_hit_result = nullptr;
    static float last_tolerance = 0.0f;
    
    // Check if we can reuse previous result (within 1/4 tolerance distance)
    float cache_distance = geometry_utils::FastDistanceSquared(world_pos, last_query_pos);
    if (cache_distance < (tolerance * tolerance * 0.0625f) && tolerance == last_tolerance) {
        return last_hit_result;
    }
    
    const Element* hit_element = nullptr;
    
    // Try optimized spatial hit detection first
    if (m_render_pipeline_ && m_render_pipeline_->IsInitialized()) {
        // Use a prefetch hint for the spatial index data
        hit_element = m_render_pipeline_->FindHitElementOptimized(world_pos, tolerance, nullptr);
    }
    
    // Cache the result for future queries
    last_query_pos = world_pos;
    last_hit_result = hit_element;
    last_tolerance = tolerance;
    
    return hit_element;
}
