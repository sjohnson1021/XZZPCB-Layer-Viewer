#include "pcb/Board.hpp"

#include <iostream>

#include "core/BoardDataManager.hpp"   // Include for implementation
#include "pcb/BoardLoaderFactory.hpp"  // Include the factory

// Include concrete element types for make_unique
#include "pcb/elements/Arc.hpp"
#include "pcb/elements/TextLabel.hpp"
#include "pcb/elements/Trace.hpp"
#include "pcb/elements/Via.hpp"
// Component.hpp and Net.hpp are already included via Board.hpp (as they are not just Element types)

// Default constructor
Board::Board() : m_is_loaded_(false)
{
    // Default initialization only
}

// Constructor that takes a file path - now just calls initialize
Board::Board(const std::string& filePath) : file_path(filePath), m_is_loaded_(false)
{
    Initialize(filePath);
}

// New initialization method that can be called by both constructor and loaders
bool Board::Initialize(const std::string& filePath)
{
    if (filePath.empty()) {
        m_error_message_ = "File path is empty.";
        m_is_loaded_ = false;
        return false;
    }

    if (filePath == "dummy_fail.pcb") {
        m_error_message_ = "This is a dummy failure to test the error modal.";
        m_is_loaded_ = false;
        return false;
    }

    // Automatically normalize coordinates after loading
    BLRect bounds = GetBoundingBox(true);
    if (bounds.w > 0 && bounds.h > 0) {
        NormalizeCoordinatesAndGetCenterOffset(bounds);
    } else {
        std::cerr << "Warning: Could not determine valid bounding box for normalization for file: " << filePath << std::endl;
    }

    // Apply board folding if enabled in BoardDataManager
    if (m_board_data_manager_ && m_board_data_manager_->IsBoardFoldingEnabled()) {
		std::cout << "Board folding enabled. Applying folding..." << std::endl;
        ApplyBoardFolding();
    }

    m_is_loaded_ = true;
    m_error_message_.clear();
    return true;
}

// --- Add Methods ---
void Board::AddArc(const Arc& arc)
{
    m_elements_by_layer[arc.GetLayerId()].emplace_back(std::make_unique<Arc>(arc));
}
void Board::AddVia(const Via& via)
{
    // Vias can span multiple layers. Add to m_elementsByLayer based on their primary layer id.
    // The IsOnLayer() check within Via itself handles multi-layer aspect for rendering/interaction.
    m_elements_by_layer[via.GetLayerId()].emplace_back(std::make_unique<Via>(via));
}
void Board::AddTrace(const Trace& trace)
{
    m_elements_by_layer[trace.GetLayerId()].emplace_back(std::make_unique<Trace>(trace));
}
void Board::AddStandaloneTextLabel(const TextLabel& label)
{
    m_elements_by_layer[label.GetLayerId()].emplace_back(std::make_unique<TextLabel>(label));
}
void Board::AddComponent(Component& component)
{
    m_elements_by_layer[Board::kBottomCompLayer].emplace_back(std::make_unique<Component>(component));
}
void Board::AddNet(const Net& net)
{
    m_nets.emplace(net.GetId(), net);  // Nets stored directly
}
void Board::AddLayer(const LayerInfo& layer)
{
    LayerInfo l = layer;
    l.is_visible = true;
    layers.push_back(l);
}

// --- Layer Access Methods ---
std::vector<Board::LayerInfo> Board::GetLayers() const
{
    return layers;
}

int Board::GetLayerCount() const
{
    return static_cast<int>(layers.size());
}

std::string Board::GetLayerName(int layerIndex) const
{
    if (layerIndex >= 0 && layerIndex < layers.size()) {
        return layers[layerIndex].name;
    }
    return "Invalid Layer Index";  // Or throw an exception
}

bool Board::IsLayerVisible(int layerIndex) const
{
    if (layerIndex >= 0 && layerIndex < layers.size()) {
        return layers[layerIndex].is_visible;
    }
    return false;  // Or throw an exception
}

void Board::SetLayerVisible(int layerIndex, bool visible)
{
    if (layerIndex >= 0 && layerIndex < layers.size()) {
        layers[layerIndex].is_visible = visible;
        // Note: We don't call BoardDataManager::SetLayerVisible here to avoid recursion
        // The BoardDataManager should be the primary source of truth for layer visibility
        // and will update the board's layer visibility directly when needed
    }
    // Else: handle error or do nothing for invalid index
}

void Board::SetLayerColor(int layerIndex, BLRgba32 color)
{
    // No-op: color is now managed by BoardDataManager
}

// --- Loading Status Methods ---
bool Board::IsLoaded() const
{
    return m_is_loaded_;
}

std::string Board::GetErrorMessage() const
{
    return m_error_message_;
}

std::string Board::GetFilePath() const
{
    return file_path;
}

const Board::LayerInfo* Board::GetLayerById(int layerId) const
{
    for (const auto& layer : layers) {
        if (layer.id == layerId) {
            return &layer;
        }
    }
    return nullptr;  // Not found
}

// Calculate the bounding box based on elements on Layer 28 (Board Outline for XZZ)
BLRect Board::GetBoundingBox(bool include_invisible_layers) const
{
    bool first = true;
    double min_x = 0, min_y = 0, max_x = 0, max_y = 0;
    const int board_outline_layer_id = 28;  // XZZ specific

    const LayerInfo* outline_layer_info = GetLayerById(board_outline_layer_id);

    if (!outline_layer_info) {
        std::cerr << "Warning: Board outline layer ID " << board_outline_layer_id << " not found in layer definitions." << std::endl;
        return BLRect(0, 0, 0, 0);  // Layer 28 not defined, cannot determine outline.
    }

    if (!include_invisible_layers && !outline_layer_info->IsVisible()) {
        // Layer 28 is not visible and we are not including invisible layers.
        return BLRect(0, 0, 0, 0);
    }

    auto expand_rect_from_point = [&](double x, double y) {
        if (first) {
            min_x = max_x = x;
            min_y = max_y = y;
            first = false;
        } else {
            if (x < min_x)
                min_x = x;
            if (x > max_x)
                max_x = x;
            if (y < min_y)
                min_y = y;
            if (y > max_y)
                max_y = y;
        }
    };

    auto expand_rect_from_blrect = [&](const BLRect& r) {
        if (r.w <= 0 || r.h <= 0)
            return;  // Skip invalid rects
        expand_rect_from_point(r.x, r.y);
        expand_rect_from_point(r.x + r.w, r.y + r.h);
    };

    auto it = m_elements_by_layer.find(board_outline_layer_id);
    if (it != m_elements_by_layer.end()) {
        for (const auto& element_ptr : it->second) {
            if (!element_ptr)
                continue;

            // All elements on the outline layer should contribute.
            // The Element::getBoundingBox() should provide the necessary extent.
            // We pass nullptr for parentComponent as these are standalone elements.
            BLRect elem_bounds = element_ptr->GetBoundingBox(nullptr);
            expand_rect_from_blrect(elem_bounds);
        }
    } else {
        std::cerr << "Warning: No elements found on board outline layer ID " << board_outline_layer_id << "." << std::endl;
    }

    if (first) {  // No elements found on layer 28, or they all had invalid bounding boxes
        std::cerr << "Warning: Could not determine board bounds from outline layer " << board_outline_layer_id << ". Resulting bounds are empty." << std::endl;
        return BLRect(0, 0, 0, 0);
    }
    return BLRect(min_x, min_y, max_x - min_x, max_y - min_y);
}

BoardPoint2D Board::NormalizeCoordinatesAndGetCenterOffset(const BLRect& original_bounds)
{
    if (original_bounds.w <= 0 || original_bounds.h <= 0) {
        // std::cerr << "NormalizeCoordinatesAndGetCenterOffset: Original bounds invalid, skipping normalization." << std::endl;
        return {0.0, 0.0};  // No valid bounds to normalize against
    }

    double offset_x = original_bounds.x + original_bounds.w / 2.0;
    double offset_y = original_bounds.y + original_bounds.h / 2.0;

    // std::cout << "Normalizing by offset: X=" << offset_x << ", Y=" << offset_y << std::endl;

    // Normalize all standalone elements in m_elementsByLayer
    for (auto& layer_pair : m_elements_by_layer) {
        for (auto& element_ptr : layer_pair.second) {
            if (element_ptr) {
                element_ptr->Translate(-offset_x, -offset_y);
            }
        }
    }

    // Normalize Components and their sub-elements (pins, component graphics, component text labels)
    // The Component::translate method should handle its children.
    // for (auto &comp_ptr : m_elementsByLayer[Board::kCompLayer])
    // {
    //     if (comp_ptr)
    //     {
    //         comp_ptr->translate(-offset_x, -offset_y);
    //     }
    // }

    // Update the board's own origin_offset to store this normalization offset
    this->origin_offset = {offset_x, offset_y};

    // The board's width and height are derived from the original_bounds,
    // they don't change due to normalization, only their position relative to (0,0).
    this->width = original_bounds.w;
    this->height = original_bounds.h;

    // std::cout << "Board dimensions after norm: W=" << this->width << ", H=" << this->height << std::endl;
    // BLRect new_bounds_check = GetBoundingBox(true);
    // std::cout << "New bounds after norm: X=" << new_bounds_check.x << " Y=" << new_bounds_check.y << " W=" << new_bounds_check.w << " H=" << new_bounds_check.h << std::endl;

    return {offset_x, offset_y};
}

// If PcbLoader is used internally by Board::ParseBoardFile (example skeleton)
// ... existing code ...

// --- GetAllVisibleElementsForInteraction (New Implementation) ---
std::vector<ElementInteractionInfo> Board::GetAllVisibleElementsForInteraction() const
{
    std::vector<ElementInteractionInfo> result;
    result.reserve(5000);  // Pre-allocate, adjust based on typical board size

    // 1. Iterate through standalone elements grouped by layer
    for (const auto& layer_pair : m_elements_by_layer) {
        const LayerInfo* layer_info = GetLayerById(layer_pair.first);
        if (layer_info && layer_info->IsVisible()) {
            for (const auto& element_ptr : layer_pair.second) {
                if (element_ptr && element_ptr->IsVisible())  // Assuming Element base has isVisible()
                {
                    result.push_back({element_ptr.get(), nullptr});
                }
            }
        } else {
            std::cout << "Board: Layer " << layer_pair.first << " is not visible or not found" << std::endl;
        }
    }

    // 2. Iterate through components and their elements (Pins, component-specific TextLabels)
    auto comp_layer_it = m_elements_by_layer.find(Board::kBottomCompLayer);
    if (comp_layer_it != m_elements_by_layer.end()) {
        for (const auto& element_ptr : comp_layer_it->second) {
            if (!element_ptr)
                continue;
            // Try to cast to Component
            const Component* comp = dynamic_cast<const Component*>(element_ptr.get());
            if (!comp)
                continue;

            const LayerInfo* comp_layer_info = GetLayerById(comp->layer);  // Component's primary layer

            if (comp_layer_info && comp_layer_info->IsVisible()) {
                // Add Pins of the component
                for (const auto& pin_ptr : comp->pins) {
                    if (!pin_ptr)
                        continue;
                    const LayerInfo* pin_layer_info = GetLayerById(pin_ptr->GetLayerId());
                    if (pin_ptr->IsVisible() && pin_layer_info && pin_layer_info->IsVisible()) {
                        result.push_back({pin_ptr.get(), comp});
                    }
                }
                // Add TextLabels of the component
                for (const auto& label_ptr : comp->text_labels) {
                    if (!label_ptr)
                        continue;
                    const LayerInfo* label_layer_info = GetLayerById(label_ptr->GetLayerId());
                    if (label_ptr->IsVisible() && label_layer_info && label_layer_info->IsVisible()) {
                        result.push_back({label_ptr.get(), comp});
                    } else {
                        std::cout << "Board: TextLabel on layer " << label_ptr->GetLayerId() << " is not visible or layer not found" << std::endl;
                    }
                }
            } else {
                std::cout << "Board: Component layer is not visible or not found" << std::endl;
            }
        }
    }
    return result;
}

const Net* Board::GetNetById(int net_id) const
{
    auto it = m_nets.find(net_id);
    if (it != m_nets.end()) {
        return &(it->second);
    }
    return nullptr;
}

void Board::SetBoardDataManager(std::shared_ptr<BoardDataManager> manager)
{
    m_board_data_manager_ = manager;
}

// --- Board Folding Implementation ---

double Board::DetectBoardCenterAxis() const
{
    // Find the center line where the board "folds" by analyzing board outline elements
    double min_x = std::numeric_limits<double>::max();
    double max_x = std::numeric_limits<double>::lowest();

    const int board_outline_layer_id = 28;  // XZZ specific board outline layer

    auto it = m_elements_by_layer.find(board_outline_layer_id);
    if (it != m_elements_by_layer.end()) {
        for (const auto& element_ptr : it->second) {
            if (!element_ptr) continue;

            BLRect bounds = element_ptr->GetBoundingBox(nullptr);
            if (bounds.w > 0 && bounds.h > 0) {
                min_x = std::min(min_x, bounds.x);
                max_x = std::max(max_x, bounds.x + bounds.w);
            }
        }
    }

    if (min_x != std::numeric_limits<double>::max() && max_x != std::numeric_limits<double>::lowest()) {
        return (min_x + max_x) / 2.0;
    }

    // Fallback: use board width center
    return width / 2.0;
}

bool Board::SegmentBelongsToTopSide(const BoardPoint2D& p1, const BoardPoint2D& p2, double center_x) const
{
    // A segment belongs to the top side if its midpoint is left of the center axis
    double midpoint_x = (p1.x + p2.x) / 2.0;
    return midpoint_x < center_x;
}

void Board::CleanDuplicateOutlineSegments()
{
    const int board_outline_layer_id = 28;
    double center_x = m_board_center_x_;

    auto it = m_elements_by_layer.find(board_outline_layer_id);
    if (it == m_elements_by_layer.end()) return;

    std::vector<std::unique_ptr<Element>> top_side_elements;

    for (auto& element_ptr : it->second) {
        if (!element_ptr) continue;

        // For now, we'll keep all elements and let the rendering handle the folding
        // In a more sophisticated implementation, we would analyze line segments
        // and remove duplicates based on their position relative to center_x
        BLRect bounds = element_ptr->GetBoundingBox(nullptr);
        double element_center_x = bounds.x + bounds.w / 2.0;

        if (element_center_x < center_x) {
            top_side_elements.push_back(std::move(element_ptr));
        }
    }

    // Replace the outline elements with only the top side ones
    it->second = std::move(top_side_elements);
}

void Board::AssignComponentSidesAndFold(double center_x)
{
    // Process components and determine their mounting side
    auto comp_layer_it = m_elements_by_layer.find(Board::kBottomCompLayer);
    if (comp_layer_it == m_elements_by_layer.end()) return;

    for (auto& element_ptr : comp_layer_it->second) {
        if (!element_ptr) continue;

        Component* comp = dynamic_cast<Component*>(element_ptr.get());
        if (!comp) continue;

        // Determine if component is on left (top) or right (bottom) side
        bool is_on_top_side = comp->center_x < center_x;

        if (!is_on_top_side) {
            // Use the new Mirror method to properly mirror the component
            // This handles the component center, pins, text labels, and graphical elements
            comp->Mirror(center_x);

            // Update component mounting side
            comp->side = MountingSide::kBottom;
			comp->layer = Board::kBottomCompLayer;
			//iterate through pins, and assign them to the top pins layer
			for (auto& pin_ptr : comp->pins) {
				if (!pin_ptr) continue;
				pin_ptr->SetLayerId(Board::kBottomPinsLayer);
				
			}
            // Log information about the component being mirrored
            std::cout << "Mirrored component " << comp->reference_designator
                      << " (pins: " << comp->pins.size() << ")" << std::endl;
        } else {
            comp->side = MountingSide::kTop;
			comp->layer = Board::kTopCompLayer;
			for (auto& pin_ptr : comp->pins) {
				if (!pin_ptr) continue;
				pin_ptr->SetLayerId(Board::kTopPinsLayer);
				
			}
        }
    }
}

void Board::ApplyBoardFolding()
{
    std::cout << "Board::ApplyBoardFolding() called" << std::endl;

    if (m_is_folded_) {
        std::cout << "Board::ApplyBoardFolding() - Already folded, skipping" << std::endl;
        return;  // Already folded
    }

    // Detect the center axis
    m_board_center_x_ = DetectBoardCenterAxis();
    std::cout << "Board::ApplyBoardFolding() - Detected center axis at X=" << m_board_center_x_ << std::endl;

    // Mirror traces and other elements from right side to left side
    for (auto& layer_pair : m_elements_by_layer) {
        int layer_id = layer_pair.first;

        // Skip component layer (handled separately) and board outline layer
        if (layer_id == Board::kBottomCompLayer || layer_id == Board::kTopCompLayer || layer_id == 28) continue;

        for (auto& element_ptr : layer_pair.second) {
            if (!element_ptr) continue;

            // Check if element is on the right side (bottom layer)
            BLRect bounds = element_ptr->GetBoundingBox(nullptr);
            double element_center_x = bounds.x + bounds.w / 2.0;
            
            if (element_center_x > m_board_center_x_) {
                // Perform element-specific geometric mirroring
                if (auto trace = dynamic_cast<Trace*>(element_ptr.get())) {
                    // For traces: Mirror both endpoints across the center axis
                    trace->x1 = 2 * m_board_center_x_ - trace->x1;
                    trace->x2 = 2 * m_board_center_x_ - trace->x2;
                    // Y coordinates remain unchanged in horizontal mirroring
                } 
                else if (auto arc = dynamic_cast<Arc*>(element_ptr.get())) {
                    // For arcs: Mirror center point and adjust angles
                    arc->center.x_ax = 2 * m_board_center_x_ - arc->center.x_ax;

                    // For horizontal mirroring, transform angles and swap start/end
                    // Original arc from start_angle to end_angle becomes
                    // arc from (180° - end_angle) to (180° - start_angle)
                    double original_start = arc->start_angle;
                    double original_end = arc->end_angle;

                    arc->start_angle = 180.0 - original_end;
                    arc->end_angle = 180.0 - original_start;

                    // Normalize angles to [0, 360) range
                    while (arc->start_angle < 0) arc->start_angle += 360.0;
                    while (arc->end_angle < 0) arc->end_angle += 360.0;
                    while (arc->start_angle >= 360.0) arc->start_angle -= 360.0;
                    while (arc->end_angle >= 360.0) arc->end_angle -= 360.0;
                }
                else if (auto via = dynamic_cast<Via*>(element_ptr.get())) {
                    // For vias: Simply mirror the center point
                    via->x = 2 * m_board_center_x_ - via->x;
                    // Y coordinate remains unchanged
                }
                else if (auto text = dynamic_cast<TextLabel*>(element_ptr.get())) {
                    // For text labels: Mirror position and flip horizontal alignment
                    text->coords.x_ax = 2 * m_board_center_x_ - text->coords.x_ax;
                    // Optionally flip text direction/alignment if needed
                    // text->horizontal_alignment = FlipAlignment(text->horizontal_alignment);
                }
                else {
                    // For any other element types: Use the generic Translate method
                    // This is a fallback for element types we haven't specifically handled
                    double mirrored_x = 2 * m_board_center_x_ - element_center_x;
                    double offset_x = mirrored_x - element_center_x;
                    element_ptr->Translate(offset_x, 0.0);
                }
            }
        }
    }

    // Handle components separately
    AssignComponentSidesAndFold(m_board_center_x_);

    // Clean up duplicate board outline segments
    CleanDuplicateOutlineSegments();

    m_is_folded_ = true;
    std::cout << "Board::ApplyBoardFolding() - Board folding completed successfully" << std::endl;
}

void Board::RevertBoardFolding()
{
    if (!m_is_folded_) return;  // Not folded

    // This is a simplified revert - in a full implementation, you'd need to
    // store the original positions and restore them
    // For now, we'll just mark as unfolded and let the board be reloaded
    m_is_folded_ = false;

    std::cout << "Board folding reverted. Consider reloading the board for full restoration." << std::endl;
}

void Board::UpdateFoldingState()
{
    std::cout << "Board::UpdateFoldingState() called" << std::endl;

    if (!m_board_data_manager_) {
        std::cout << "Board::UpdateFoldingState() - No BoardDataManager, skipping" << std::endl;
        return;
    }

    bool should_be_folded = m_board_data_manager_->IsBoardFoldingEnabled();
    std::cout << "Board::UpdateFoldingState() - Should be folded: " << (should_be_folded ? "true" : "false") << std::endl;
    std::cout << "Board::UpdateFoldingState() - Currently folded: " << (m_is_folded_ ? "true" : "false") << std::endl;

    if (should_be_folded && !m_is_folded_) {
        std::cout << "Board folding enabled. Applying folding..." << std::endl;
        ApplyBoardFolding();
    } else if (!should_be_folded && m_is_folded_) {
        std::cout << "Board folding disabled. Reverting folding..." << std::endl;
        RevertBoardFolding();
    } else {
        std::cout << "Board::UpdateFoldingState() - No folding state change needed" << std::endl;
    }
}

void Board::ApplyGlobalTransformation(bool mirror_horizontally)
{
    std::cout << "Board::ApplyGlobalTransformation() called with mirror_horizontally="
              << (mirror_horizontally ? "true" : "false") << std::endl;

    if (!mirror_horizontally) {
        std::cout << "Board::ApplyGlobalTransformation() - No mirroring requested, skipping" << std::endl;
        return;
    }

    // Get board bounds to determine the center axis for mirroring
    BLRect board_bounds = GetBoundingBox(false);
    if (board_bounds.w <= 0 && board_bounds.h <= 0) {
        std::cout << "Board::ApplyGlobalTransformation() - No valid board bounds, skipping" << std::endl;
        return;
    }

    double center_x = board_bounds.x + board_bounds.w / 2.0;
    std::cout << "Board::ApplyGlobalTransformation() - Using center axis at X=" << center_x << std::endl;

    // Mirror non-component elements in all layers
    for (auto& layer_pair : m_elements_by_layer) {
        int layer_id = layer_pair.first;

        // Skip component layers and pin layers - they're handled separately
        if (layer_id == Board::kTopCompLayer || layer_id == Board::kBottomCompLayer ||
            layer_id == Board::kTopPinsLayer || layer_id == Board::kBottomPinsLayer) {
            continue;
        }

        std::cout << "Board::ApplyGlobalTransformation() - Processing layer " << layer_id
                  << " with " << layer_pair.second.size() << " elements" << std::endl;

        for (auto& element_ptr : layer_pair.second) {
            if (!element_ptr) continue;

            // Apply mirror transformation to non-component elements (traces, arcs, vias, etc.)
            element_ptr->Mirror(center_x);
        }
    }

    // Components are stored in m_elements_by_layer at component layers
    // Mirror components in both top and bottom component layers
    std::vector<int> component_layers = {Board::kTopCompLayer, Board::kBottomCompLayer};

    for (int comp_layer : component_layers) {
        auto comp_layer_it = m_elements_by_layer.find(comp_layer);
        if (comp_layer_it == m_elements_by_layer.end()) continue;

        std::cout << "Board::ApplyGlobalTransformation() - Processing component layer " << comp_layer
                  << " with " << comp_layer_it->second.size() << " components" << std::endl;

        for (auto& element_ptr : comp_layer_it->second) {
            if (!element_ptr) continue;

            // Check if this element is a Component
            if (auto comp_ptr = dynamic_cast<Component*>(element_ptr.get())) {
                std::cout << "Board::ApplyGlobalTransformation() - Mirroring component "
                          << comp_ptr->reference_designator << std::endl;
                comp_ptr->Mirror(center_x);
            }
        }
    }

    std::cout << "Board::ApplyGlobalTransformation() - Global transformation completed" << std::endl;
}
