#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory> // For potential use of smart pointers

#include "elements/Arc.hpp"
#include "elements/Component.hpp"
#include "elements/Net.hpp"
#include "elements/Trace.hpp"
#include "elements/Via.hpp"
#include "elements/TextLabel.hpp" // For standalone text not part of components

// Structure to define a layer
struct LayerInfo {
    enum class LayerType {
        Signal,
        PowerPlane, // Could be positive or negative plane
        Silkscreen,
        SolderMask,
        SolderPaste,
        Drill,
        Mechanical,
        BoardOutline,
        Comment,
        Other
    };

    int id = 0;                     // Original ID from file if applicable, or internal ID
    std::string name;               // e.g., "TopLayer", "BottomLayer", "SilkscreenTop"
    LayerType type = LayerType::Other;
    bool is_visible = true;
    // Color color; // Optional: for default display color
    // double thickness; // Optional: physical thickness of the layer

    LayerInfo(int i, const std::string& n, LayerType t) : id(i), name(n), type(t) {}
};

class Board {
public:
    Board() = default;

    // --- Board Metadata ---
    std::string board_name;
    std::string file_path; // Path to the original PCB file

    // Board dimensions (could be calculated from elements or read from file)
    double width = 0.0;
    double height = 0.0;
    // Point2D origin_offset = {0.0, 0.0}; // If coordinates need global adjustment. Requires Point2D to be defined or included.
    // For now, let's assume Point2D from Component.hpp is not directly used here or define a simple one.
    struct BoardPoint2D { double x = 0.0; double y = 0.0; };
    BoardPoint2D origin_offset = {0.0, 0.0};


    std::vector<LayerInfo> layers;

    // --- PCB Elements ---
    // Using value semantics for simplicity here.
    // Smart pointers (e.g., std::vector<std::unique_ptr<Arc>>) could be used
    // if elements are large, polymorphic, or have complex ownership.

    std::vector<Arc> arcs;
    std::vector<Via> vias;
    std::vector<Trace> traces;
    std::vector<TextLabel> standalone_text_labels; // Text not directly part of a component
    std::vector<Component> components;
    std::unordered_map<int, Net> nets; // Keyed by Net ID for quick lookup

    // --- Methods ---
    // Methods to add elements
    void addArc(const Arc& arc) { arcs.push_back(arc); }
    void addVia(const Via& via) { vias.push_back(via); }
    void addTrace(const Trace& trace) { traces.push_back(trace); }
    void addStandaloneTextLabel(const TextLabel& label) { standalone_text_labels.push_back(label); }
    void addComponent(const Component& component) { components.push_back(component); }
    void addNet(const Net& net) { nets.emplace(net.id, net); }
    void addLayer(const LayerInfo& layer) { layers.push_back(layer); }

    // Methods to retrieve elements (examples)
    const std::vector<Component>& getComponents() const { return components; }
    const Net* getNetById(int net_id) const {
        auto it = nets.find(net_id);
        return (it != nets.end()) ? &(it->second) : nullptr;
    }

    // Methods for board-level operations (e.g., calculate extents)
    // void calculateBoardDimensions();
}; 