#include "PcbDetailsWindow.hpp"
#include <variant>
#include <string> // For std::to_string
#include "imgui.h"
#include "../../pcb/Board.hpp"
#include "../../pcb/elements/Component.hpp"
#include "../../pcb/elements/Pin.hpp"
#include "../../pcb/elements/TextLabel.hpp"
#include <vector>

// Constructor
PcbDetailsWindow::PcbDetailsWindow() : current_board_(nullptr), is_visible_(false) {
}

// Added methods
void PcbDetailsWindow::setBoard(std::shared_ptr<Board> board) {
    current_board_ = board;
}

void PcbDetailsWindow::SetVisible(bool visible) {
    is_visible_ = visible;
}

bool PcbDetailsWindow::IsWindowVisible() const {
    return is_visible_;
}

void PcbDetailsWindow::render() {
    if (!is_visible_ || !current_board_) {
        return;
    }

    if (ImGui::Begin("PCB Details", &is_visible_)) {
        displayBasicInfo(current_board_.get());
        displayLayers(current_board_.get());
        displayNets(current_board_.get());
        displayComponents(current_board_.get());
        displayStandaloneElements(current_board_.get());
    }
    ImGui::End();
}

void PcbDetailsWindow::displayBasicInfo(const Board* boardData) {
    ImGui::Text("Board Name: %s", boardData->board_name.c_str());
    ImGui::Text("File Path: %s", boardData->file_path.c_str());
    ImGui::Text("Dimensions: %.2f x %.2f", boardData->width, boardData->height); // Assuming width/height get populated
    ImGui::Separator();
}

void PcbDetailsWindow::displayLayers(const Board* boardData) {
    for (const auto& layer : boardData->layers) {
        ImGui::Text("ID: %d, Name: %s, Type: %d, Visible: %s", 
                    layer.id, layer.name.c_str(), static_cast<int>(layer.type), layer.is_visible ? "Yes" : "No");
    }
}

void PcbDetailsWindow::displayNets(const Board* boardData) {
    for (const auto& pair : boardData->nets) {
        const Net& net = pair.second;
        ImGui::Text("ID: %d, Name: %s", net.id, net.name.c_str());
    }
}

void PcbDetailsWindow::displayPadShape(const PadShape& shape) {
    std::visit([](const auto& s) {
        using T = std::decay_t<decltype(s)>;
        if constexpr (std::is_same_v<T, CirclePad>) {
            ImGui::TextUnformatted(("Shape: Circle, Radius: " + std::to_string(s.radius)).c_str());
        } else if constexpr (std::is_same_v<T, RectanglePad>) {
            ImGui::TextUnformatted(("Shape: Rectangle, W: " + std::to_string(s.width) + ", H: " + std::to_string(s.height)).c_str());
        } else if constexpr (std::is_same_v<T, CapsulePad>) {
            ImGui::TextUnformatted(("Shape: Capsule, W: " + std::to_string(s.width) + ", H: " + std::to_string(s.height)).c_str());
        }
    }, shape);
}

void PcbDetailsWindow::displayPins(const std::vector<Pin>& pins) {
    for (const auto& pin : pins) {
        if (ImGui::TreeNodeEx(("Pin: " + pin.pin_name + " (Net: " + std::to_string(pin.net_id) + ")").c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("Coords: (%.2f, %.2f), Layer: %d, Side: %d", pin.x_coord, pin.y_coord, pin.layer, pin.side);
            displayPadShape(pin.pad_shape);
            if (!pin.diode_reading.empty()) {
                ImGui::Text("Diode: %s", pin.diode_reading.c_str());
            }
            ImGui::TreePop();
        }
    }
}

void PcbDetailsWindow::displayGraphicalElements(const std::vector<LineSegment>& elements) {
    for (size_t i = 0; i < elements.size(); ++i) {
        const auto& seg = elements[i];
        if (ImGui::TreeNodeEx(("Segment " + std::to_string(i)).c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("Layer: %d, Thickness: %.2f", seg.layer, seg.thickness);
            ImGui::Text("Start: (%.2f, %.2f), End: (%.2f, %.2f)", seg.start.x, seg.start.y, seg.end.x, seg.end.y);
            ImGui::TreePop();
        }
    }
}

void PcbDetailsWindow::displayComponents(const Board* boardData) {
    for (const auto& comp : boardData->components) {
        std::string compNodeName = comp.reference_designator + " (" + comp.value + ") - " + comp.footprint_name;
        if (ImGui::TreeNode(compNodeName.c_str())) {
            ImGui::Text("Pos: (%.2f, %.2f), Layer: %d, Rot: %.1f deg", comp.center_x, comp.center_y, comp.layer, comp.rotation);
            ImGui::Text("Type: %d, Side: %d", static_cast<int>(comp.type), static_cast<int>(comp.side));
            
            if (ImGui::TreeNodeEx("Pins", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed)) {
                displayPins(comp.pins);
                ImGui::TreePop();
            }
            if (ImGui::TreeNodeEx("Labels", ImGuiTreeNodeFlags_Framed)) {
                for(const auto& lbl : comp.text_labels) {
                     ImGui::Text("L%d (%.1f,%.1f) S%.1f: %s", lbl.layer, lbl.x, lbl.y, lbl.font_size, lbl.text_content.c_str());
                }
                ImGui::TreePop();
            }
            if (ImGui::TreeNodeEx("Graphical Elements", ImGuiTreeNodeFlags_Framed)) {
                displayGraphicalElements(comp.graphical_elements);
                ImGui::TreePop();
            }
            ImGui::TreePop();
        }
    }
}

void PcbDetailsWindow::displayStandaloneElements(const Board* boardData) {
    ImGui::Text("Arcs: %zu", boardData->arcs.size());
    // Could add details for Arcs if needed
    ImGui::Text("Vias: %zu", boardData->vias.size());
    // Could add details for Vias if needed
    ImGui::Text("Traces: %zu", boardData->traces.size());
    // Could add details for Traces if needed
    ImGui::Text("Standalone Text Labels: %zu", boardData->standalone_text_labels.size());
    for(const auto& lbl : boardData->standalone_text_labels) {
        ImGui::Text("  L%d (%.1f,%.1f) S%.1f: %s", lbl.layer, lbl.x, lbl.y, lbl.font_size, lbl.text_content.c_str());
    }
} 