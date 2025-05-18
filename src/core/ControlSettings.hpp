#pragma once

#include <string>
#include "core/InputActions.hpp" // Include the new header
#include <map> // For future keybind management

// Forward declaration for potential key types if we make it more complex
// enum class InputAction { OpenFile, ResetView, ... };

struct ControlSettings {
    // Navigation settings
    bool m_freeRotation = false; // false = snap to 90 degrees, true = free continuous rotation
    float m_snapRotationAngle = 90.0f; // Angle to snap to in degrees
    bool m_rotateAroundCursor = false;      // Rotate around the mouse cursor

    // Keybinds map
    KeybindMap m_keybinds;

    ControlSettings(); // Constructor to initialize defaults

    // Methods to manage keybinds
    KeyCombination GetKeybind(InputAction action) const;
    void SetKeybind(InputAction action, KeyCombination keyCombination);
    void LoadKeybindsFromConfig(const class Config& config); // Forward declare Config
    void SaveKeybindsToConfig(class Config& config) const;   // Forward declare Config
    void ResetKeybindsToDefault();

private:
    void InitializeDefaultKeybinds();

    // Potentially methods to load/save settings in the future
    // void LoadFromFile(const std::string& path);
    // void SaveToFile(const std::string& path);
}; 