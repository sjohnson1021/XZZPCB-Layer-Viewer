#pragma once

#include "core/InputActions.hpp"  // Include the new header
#include <vector>
#include <array>

// Enum for different element types that can be interacted with
enum class ElementInteractionType : uint8_t {
    kPins = 0,
    kComponents,
    kTraces,
    kVias,
    kTextLabels,
    kCount  // Keep last for array sizing
};

// Helper function to convert ElementInteractionType to string
const char* ElementInteractionTypeToString(ElementInteractionType type);

// Forward declaration for potential key types if we make it more complex
// enum class InputAction { OpenFile, ResetView, ... };

class ControlSettings {
public:
    // Navigation settings
    bool m_free_rotation = false;         // false = snap to 90 degrees, true = free continuous rotation
    float m_snap_rotation_angle = 90.0F;  // Angle to snap to in degrees
    bool m_rotate_around_cursor = false;  // Rotate around the mouse cursor

    // Speed controls (capped at reasonable limits)
    float m_zoom_sensitivity = 1.1F;      // Mouse wheel zoom factor (1.05 to 2.0)
    float m_pan_speed_multiplier = 1.0F;  // Pan speed multiplier (0.1 to 5.0)

    // Keybinds map
    KeybindMap m_keybinds;

    // Element interaction priority (lower index = higher priority)
    std::array<ElementInteractionType, static_cast<size_t>(ElementInteractionType::kCount)> m_element_priority_order;

    ControlSettings();  // Constructor to initialize defaults

    // Methods to manage keybinds
    [[nodiscard]] KeyCombination GetKeybind(InputAction action) const;
    void SetKeybind(InputAction action, KeyCombination key_combination);
    void LoadKeybindsFromConfig(const class Config& config);  // Forward declare Config
    void SaveKeybindsToConfig(class Config& config);          // Forward declare Config
    void ResetKeybindsToDefault();

    // Methods to manage element interaction priority
    [[nodiscard]] const std::array<ElementInteractionType, static_cast<size_t>(ElementInteractionType::kCount)>& GetElementPriorityOrder() const;
    void SetElementPriorityOrder(const std::array<ElementInteractionType, static_cast<size_t>(ElementInteractionType::kCount)>& priority_order);
    void ResetElementPriorityToDefault();

    // Methods to manage all settings (keybinds + speed controls + interaction priority)
    void LoadSettingsFromConfig(const class Config& config);
    void SaveSettingsToConfig(class Config& config);

private:
    void InitializeDefaultKeybinds();
    void InitializeDefaultElementPriority();

    // Potentially methods to load/save settings in the future
    // void LoadFromFile(const std::string& path);
    // void SaveToFile(const std::string& path);
};
