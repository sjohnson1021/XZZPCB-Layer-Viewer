#pragma once

#include "core/InputActions.hpp"  // Include the new header

// Forward declaration for potential key types if we make it more complex
// enum class InputAction { OpenFile, ResetView, ... };

struct ControlSettings {
    // Navigation settings
    bool m_free_rotation = false;         // false = snap to 90 degrees, true = free continuous rotation
    float m_snap_rotation_angle = 90.0F;  // Angle to snap to in degrees
    bool m_rotate_around_cursor = false;  // Rotate around the mouse cursor

    // Speed controls (capped at reasonable limits)
    float m_zoom_sensitivity = 1.1F;      // Mouse wheel zoom factor (1.05 to 2.0)
    float m_pan_speed_multiplier = 1.0F;  // Pan speed multiplier (0.1 to 5.0)

    // Keybinds map
    KeybindMap m_keybinds;

    ControlSettings();  // Constructor to initialize defaults

    // Methods to manage keybinds
    [[nodiscard]] KeyCombination GetKeybind(InputAction action) const;
    void SetKeybind(InputAction action, KeyCombination key_combination);
    void LoadKeybindsFromConfig(const class Config& config);  // Forward declare Config
    void SaveKeybindsToConfig(class Config& config);          // Forward declare Config
    void ResetKeybindsToDefault();

    // Methods to manage all settings (keybinds + speed controls)
    void LoadSettingsFromConfig(const class Config& config);
    void SaveSettingsToConfig(class Config& config);

private:
    void InitializeDefaultKeybinds();

    // Potentially methods to load/save settings in the future
    // void LoadFromFile(const std::string& path);
    // void SaveToFile(const std::string& path);
};
