#include "PcbDetailsWindow.hpp"

#include <cstddef>
#include <memory>
#include <variant>
#include <vector>

#include "imgui.h"

#include "pcb/Board.hpp"
#include "pcb/elements/Arc.hpp"
#include "pcb/elements/Component.hpp"
#include "pcb/elements/Pin.hpp"
#include "pcb/elements/TextLabel.hpp"
#include "pcb/elements/Trace.hpp"
#include "pcb/elements/Via.hpp"

// Constructor
PcbDetailsWindow::PcbDetailsWindow() : current_board_(nullptr) {}

// Added methods
void PcbDetailsWindow::SetBoard(std::shared_ptr<Board> board)
{
    current_board_ = board;
}

void PcbDetailsWindow::SetVisible(bool visible)
{
    is_visible_ = visible;
}

bool PcbDetailsWindow::IsWindowVisible() const
{
    return is_visible_;
}

void PcbDetailsWindow::Render()
{
    if (!is_visible_ || !current_board_) {
        return;
    }

    bool window_open = false;
    window_open = ImGui::Begin("PCB Details", &is_visible_);

    if (window_open) {
        DisplayBasicInfo(current_board_.get());
        DisplayLayers(current_board_.get());
        DisplayNets(current_board_.get());
        DisplayComponents(current_board_.get());
        DisplayStandaloneElements(current_board_.get());
    }

    ImGui::End();  // Always call End() to match Begin()
}

void PcbDetailsWindow::DisplayBasicInfo(const Board* board_data)
{
    ImGui::Text("Board Name: %s", board_data->board_name.c_str());
    ImGui::Text("File Path: %s", board_data->file_path.c_str());
    ImGui::Text("Dimensions: %.2f x %.2f", board_data->width, board_data->height);  // Assuming width/height get populated
    ImGui::Separator();
}

void PcbDetailsWindow::DisplayLayers(const Board* board_data)
{
    for (const auto& layer : board_data->layers) {
        ImGui::Text("ID: %d, Name: %s, Type: %d, Visible: %s", layer.id, layer.name.c_str(), static_cast<int>(layer.type), layer.is_visible ? "Yes" : "No");
    }
}

void PcbDetailsWindow::DisplayNets(const Board* board_data)
{
    if (ImGui::TreeNodeEx("Nets", ImGuiTreeNodeFlags_DefaultOpen)) {
        for (const auto& pair : board_data->m_nets) {
            const Net& net = pair.second;
            ImGui::Text("ID: %d, Name: %s", net.GetId(), net.GetName().c_str());
        }
        ImGui::TreePop();
    }
}

void PcbDetailsWindow::DisplayPadShape(const PadShape& shape)
{
    std::visit(
        [](const auto& s) {
            using T = std::decay_t<decltype(s)>;
            if constexpr (std::is_same_v<T, CirclePad>) {
                ImGui::TextUnformatted(("Shape: Circle, Radius: " + std::to_string(s.radius)).c_str());
            } else if constexpr (std::is_same_v<T, RectanglePad>) {
                ImGui::TextUnformatted(("Shape: Rectangle, W: " + std::to_string(s.width) + ", H: " + std::to_string(s.height)).c_str());
            } else if constexpr (std::is_same_v<T, CapsulePad>) {
                ImGui::TextUnformatted(("Shape: Capsule, W: " + std::to_string(s.width) + ", H: " + std::to_string(s.height)).c_str());
            }
        },
        shape);
}

void PcbDetailsWindow::DisplayPins(const Board* board_data, const std::vector<std::unique_ptr<Pin>>& pins)
{
    for (const auto& pin_ptr : pins) {
        if (!pin_ptr)
            continue;
        const Pin& pin = *pin_ptr;

        std::string net_info_str = "Net ID: " + std::to_string(pin.GetNetId());
        if (board_data) {
            const Net* net = board_data->GetNetById(pin.GetNetId());
            if (net) {
                net_info_str = "Net: " + (net->GetName().empty() ? "[Unnamed]" : net->GetName()) + " (ID: " + std::to_string(pin.GetNetId()) + ")";
            } else if (pin.GetNetId() != -1) {
                net_info_str = "Net ID: " + std::to_string(pin.GetNetId()) + " [Not Found]";
            } else {
                net_info_str = "No Net";
            }
        }

        if (ImGui::TreeNodeEx(("Pin: " + pin.pin_name + " (" + net_info_str + ")").c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("Coords: (%.2f, %.2f), Layer: %d, Side: %d", pin.coords.x_ax, pin.coords.y_ax, pin.GetLayerId(), pin.side);
            DisplayPadShape(pin.pad_shape);
            if (!pin.diode_reading.empty()) {
                ImGui::Text("Diode: %s", pin.diode_reading.c_str());
            }
            ImGui::TreePop();
        }
    }
}

void PcbDetailsWindow::DisplayGraphicalElements(const std::vector<LineSegment>& elements)
{
    for (size_t i = 0; i < elements.size(); ++i) {
        const auto& seg = elements[i];
        if (ImGui::TreeNodeEx(("Segment " + std::to_string(i)).c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("Layer: %d, Thickness: %.2f", seg.layer, seg.thickness);
            ImGui::Text("Start: (%.2f, %.2f), End: (%.2f, %.2f)", seg.start.x_ax, seg.start.y_ax, seg.end.x_ax, seg.end.y_ax);
            ImGui::TreePop();
        }
    }
}

void PcbDetailsWindow::DisplayComponents(const Board* board_data)
{
    if (ImGui::TreeNodeEx("Components", ImGuiTreeNodeFlags_DefaultOpen)) {
        std::vector<int> comp_layers = {Board::kTopCompLayer, Board::kBottomCompLayer};
        for (int layer_id : comp_layers) {
            auto comp_layer_it = board_data->m_elements_by_layer.find(layer_id);
            if (comp_layer_it != board_data->m_elements_by_layer.end()) {
                for (const auto& element_ptr : comp_layer_it->second) {
                    if (!element_ptr)
                        continue;
                    const Component* comp = dynamic_cast<const Component*>(element_ptr.get());
                    if (!comp)
                        continue;

                std::string compNodeName = comp->reference_designator + " (" + comp->value + ") - " + comp->footprint_name;
                if (ImGui::TreeNode(compNodeName.c_str())) {
                    ImGui::Text("Pos: (%.2f, %.2f), Layer: %d, Rot: %.1f deg", comp->center_x, comp->center_y, comp->layer, comp->rotation);
                    ImGui::Text("Type: %d, Side: %d", static_cast<int>(comp->type), static_cast<int>(comp->side));

                    if (ImGui::TreeNodeEx("Pins", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed)) {
                        DisplayPins(board_data, comp->pins);
                        ImGui::TreePop();
                    }
                    if (ImGui::TreeNodeEx("Labels", ImGuiTreeNodeFlags_Framed)) {
                        for (const auto& lbl_ptr : comp->text_labels) {
                            if (!lbl_ptr)
                                continue;
                            const TextLabel& lbl = *lbl_ptr;
                            ImGui::Text("L%d (%.1f,%.1f) S%.1f: %s", lbl.GetLayerId(), lbl.coords.x_ax, lbl.coords.y_ax, lbl.font_size, lbl.text_content.c_str());
                        }
                        ImGui::TreePop();
                    }
                    if (ImGui::TreeNodeEx("Graphical Elements", ImGuiTreeNodeFlags_Framed)) {
                        DisplayGraphicalElements(comp->graphical_elements);
                        ImGui::TreePop();
                    }
                    ImGui::TreePop();
                }
                }
            }
        }
        ImGui::TreePop();
    }
}

void PcbDetailsWindow::DisplayStandaloneElements(const Board* board_data)
{
    if (board_data == nullptr) {
        return;
    }

    std::vector<ElementInteractionInfo> all_elements = board_data->GetAllVisibleElementsForInteraction();

    if (ImGui::TreeNodeEx("Standalone Elements", ImGuiTreeNodeFlags_DefaultOpen)) {
        int arc_count = 0;
        int via_count = 0;
        int trace_count = 0;
        int label_count = 0;
        for (const auto& info : all_elements) {
            if (info.parent_component)
                continue;  // Skip elements belonging to a component
            if (!info.element)
                continue;

            switch (info.element->GetElementType()) {
                case ElementType::kArc:
                    arc_count++;
                    break;
                case ElementType::kVia:
                    via_count++;
                    break;
                case ElementType::kTrace:
                    trace_count++;
                    break;
                case ElementType::kTextLabel:
                    label_count++;
                    break;
                default:
                    break;
            }
        }

        if (ImGui::TreeNodeEx(("Arcs (" + std::to_string(arc_count) + ")").c_str())) {
            for (const auto& info : all_elements) {
                if (info.parent_component || !info.element || info.element->GetElementType() != ElementType::kArc)
                    continue;
                const Arc* arc = dynamic_cast<const Arc*>(info.element);
                if (!arc)
                    continue;

                std::string net_info_str = "Net ID: " + std::to_string(arc->GetNetId());
                const Net* net = board_data->GetNetById(arc->GetNetId());
                if (net)
                    net_info_str = "Net: " + (net->GetName().empty() ? "[Unnamed]" : net->GetName()) + " (ID: " + std::to_string(arc->GetNetId()) + ")";
                else if (arc->GetNetId() != -1)
                    net_info_str = "Net ID: " + std::to_string(arc->GetNetId()) + " [Not Found]";
                else
                    net_info_str = "No Net";

                if (ImGui::TreeNodeEx(("Arc##" + std::to_string(reinterpret_cast<uintptr_t>(arc)) + " (" + net_info_str + ")").c_str())) {
                    ImGui::Text("Layer: %d", arc->GetLayerId());
                    ImGui::Text("Center: (%.2f, %.2f), Radius: %.2f", arc->GetCenterX(), arc->GetCenterY(), arc->GetRadius());
                    ImGui::Text("Angles: Start %.1f°, End %.1f°", arc->GetStartAngle(), arc->GetEndAngle());
                    ImGui::Text("Thickness: %.2f", arc->GetThickness());
                    ImGui::TreePop();
                }
            }
            ImGui::TreePop();
        }

        if (ImGui::TreeNodeEx(("Vias (" + std::to_string(via_count) + ")").c_str())) {
            for (const auto& info : all_elements) {
                if (info.parent_component || !info.element || info.element->GetElementType() != ElementType::kVia)
                    continue;
                const Via* via = dynamic_cast<const Via*>(info.element);
                if (!via)
                    continue;

                std::string net_info_str = "Net ID: " + std::to_string(via->GetNetId());
                const Net* net = board_data->GetNetById(via->GetNetId());
                if (net)
                    net_info_str = "Net: " + (net->GetName().empty() ? "[Unnamed]" : net->GetName()) + " (ID: " + std::to_string(via->GetNetId()) + ")";
                else if (via->GetNetId() != -1)
                    net_info_str = "Net ID: " + std::to_string(via->GetNetId()) + " [Not Found]";
                else
                    net_info_str = "No Net";

                if (ImGui::TreeNodeEx(("Via##" + std::to_string(reinterpret_cast<uintptr_t>(via)) + " (" + net_info_str + ")").c_str())) {
                    ImGui::Text("Coords: (%.2f, %.2f)", via->GetX(), via->GetY());
                    ImGui::Text("Layers: %d to %d (Primary: %d)", via->GetLayerFrom(), via->GetLayerTo(), via->GetLayerId());
                    ImGui::Text("Drill Diameter: %.2f", via->GetDrillDiameter());
                    ImGui::Text("Pad Radius (From): %.2f, Pad Radius (To): %.2f", via->GetPadRadiusFrom(), via->GetPadRadiusTo());
                    if (!via->GetOptionalText().empty()) {
                        ImGui::Text("Text: %s", via->GetOptionalText().c_str());
                    }
                    ImGui::TreePop();
                }
            }
            ImGui::TreePop();
        }

        if (ImGui::TreeNodeEx(("Traces (" + std::to_string(trace_count) + ")").c_str())) {
            for (const auto& info : all_elements) {
                if (info.parent_component || !info.element || info.element->GetElementType() != ElementType::kTrace)
                    continue;
                const Trace* trace = dynamic_cast<const Trace*>(info.element);
                if (!trace)
                    continue;

                std::string net_info_str = "Net ID: " + std::to_string(trace->GetNetId());
                const Net* net = board_data->GetNetById(trace->GetNetId());
                if (net)
                    net_info_str = "Net: " + (net->GetName().empty() ? "[Unnamed]" : net->GetName()) + " (ID: " + std::to_string(trace->GetNetId()) + ")";
                else if (trace->GetNetId() != -1)
                    net_info_str = "Net ID: " + std::to_string(trace->GetNetId()) + " [Not Found]";
                else
                    net_info_str = "No Net";

                if (ImGui::TreeNodeEx(("Trace##" + std::to_string(reinterpret_cast<uintptr_t>(trace)) + " (" + net_info_str + ")").c_str())) {
                    ImGui::Text("Layer: %d", trace->GetLayerId());
                    ImGui::Text("Start: (%.2f, %.2f), End: (%.2f, %.2f)", trace->GetStartX(), trace->GetStartY(), trace->GetEndX(), trace->GetEndY());
                    ImGui::Text("Width: %.2f", trace->GetWidth());
                    ImGui::TreePop();
                }
            }
            ImGui::TreePop();
        }

        if (ImGui::TreeNodeEx(("Standalone Text Labels (" + std::to_string(label_count) + ")").c_str())) {
            for (const auto& info : all_elements) {
                if (info.parent_component || !info.element || info.element->GetElementType() != ElementType::kTextLabel)
                    continue;
                const TextLabel* lbl = dynamic_cast<const TextLabel*>(info.element);
                if (!lbl)
                    continue;

                std::string net_info_str = "Net ID: " + std::to_string(lbl->GetNetId());
                // Text labels usually don't have nets, but handle if they do.
                if (lbl->GetNetId() != -1) {
                    const Net* net = board_data->GetNetById(lbl->GetNetId());
                    if (net)
                        net_info_str = "Net: " + (net->GetName().empty() ? "[Unnamed]" : net->GetName()) + " (ID: " + std::to_string(lbl->GetNetId()) + ")";
                    else
                        net_info_str = "Net ID: " + std::to_string(lbl->GetNetId()) + " [Not Found]";
                } else {
                    net_info_str = "No Net";
                }

                if (ImGui::TreeNodeEx(("Label##" + std::to_string(reinterpret_cast<uintptr_t>(lbl)) + " (" + net_info_str + ")").c_str())) {
                    ImGui::Text("Layer: %d", lbl->GetLayerId());
                    ImGui::Text("Position: (%.2f, %.2f)", lbl->coords.x_ax, lbl->coords.y_ax);
                    ImGui::Text("Font Size: %.2f, Scale: %.2f, Rotation: %.1f°", lbl->font_size, lbl->scale, lbl->rotation);
                    ImGui::Text("Family: %s", lbl->font_family.empty() ? "[Default]" : lbl->font_family.c_str());
                    ImGui::TextWrapped("Content: %s", lbl->text_content.c_str());
                    ImGui::TreePop();
                }
            }
            ImGui::TreePop();
        }
        ImGui::TreePop();  // End of "Standalone Elements"
    }
}
