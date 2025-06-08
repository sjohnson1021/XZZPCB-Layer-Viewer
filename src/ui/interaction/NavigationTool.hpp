#pragma once

#include <string>  // For m_hoveredElementInfo
#include <vector>  // For cached elements

#include "imgui.h"

#include "core/BoardDataManager.hpp"  // Need full declaration for BoardSide enum
#include "pcb/Board.hpp"
#include "ui/interaction/InteractionTool.hpp"
#include "utils/Vec2.hpp"
// Forward declaration
class ControlSettings;

class NavigationTool : public InteractionTool
{
public:
    NavigationTool(std::shared_ptr<Camera> camera, std::shared_ptr<Viewport> viewport, std::shared_ptr<ControlSettings> control_settings, std::shared_ptr<BoardDataManager> board_data_manager);
    ~NavigationTool() override = default;

    void ProcessInput(ImGuiIO& io, bool is_viewport_focused, bool is_viewport_hovered, ImVec2 viewport_top_left, ImVec2 viewport_size) override;

    void OnActivated() override;
    void OnDeactivated() override;

    // Selection specific methods
    [[nodiscard]] int GetSelectedNetId() const;
    void ClearSelection();

    // Configuration for the tool, if any
    // void SetPanSpeed(float speed) { m_panSpeed = speed; }
    // void SetZoomSensitivity(float sensitivity) { m_zoomSensitivity = sensitivity; }

private:
    std::shared_ptr<ControlSettings> m_control_settings_;
    std::shared_ptr<BoardDataManager> m_board_data_manager_;

    // --- New members for hover and selection ---
    std::string m_hovered_element_info_;
    bool m_is_hovering_element_ = false;
    int m_selected_net_id_ = -1;  // -1 indicates no net is selected
    // Potentially store more info about the selected element if needed
    // --- End new members ---

    // --- Performance optimization: Element caching system ---
    mutable std::vector<ElementInteractionInfo> m_cached_interactive_elements_;
    mutable bool m_cache_valid_ = false;

    // Cache state tracking for invalidation detection
    mutable BoardDataManager::BoardSide m_cached_board_side_ = BoardDataManager::BoardSide::kBoth;
    mutable std::vector<bool> m_cached_layer_visibility_;
    mutable std::shared_ptr<const Board> m_cached_board_;

    // Cache management methods
    void InvalidateElementCache() const;
    void UpdateElementCache() const;
    const std::vector<ElementInteractionInfo>& GetCachedInteractiveElements() const;
    bool HasCacheInvalidatingChanges() const;
    // --- End caching system ---

    // TransformMousePositionForBoardFlip method removed - no longer needed

    // Input state specific to navigation, if needed outside of ProcessInput scope
    // e.g., bool m_isPanning;

    // Default pan speed, could be configurable
    // float m_panSpeed = 10.0f;
    // float m_zoomSensitivity = 1.1f;
    // float m_rotationSpeed = 1.0f; // Degrees per pixel dragged or per key press
};
