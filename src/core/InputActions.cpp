#include "core/InputActions.hpp"
#include "imgui.h" // For ImGui::GetKeyName and ImGuiKey names
#include <sstream>
#include <algorithm> // For std::transform
#include <vector> // For splitting string

const char* InputActionToString(InputAction action) {
    switch (action) {
        case InputAction::PanUp:         return "Pan Up";
        case InputAction::PanLeft:       return "Pan Left";
        case InputAction::PanDown:       return "Pan Down";
        case InputAction::PanRight:      return "Pan Right";
        case InputAction::RotateLeft:    return "Rotate Left (View)";
        case InputAction::RotateRight:   return "Rotate Right (View)";
        case InputAction::ZoomIn:        return "Zoom In";
        case InputAction::ZoomOut:       return "Zoom Out";
        case InputAction::ResetView:     return "Reset View";
        default:                         return "Unknown Action";
    }
}

std::string KeyCombination::ToString() const {
    if (key == ImGuiKey_None) {
        return "Unbound";
    }
    std::stringstream ss;
    if (ctrl) ss << "Ctrl+";
    if (shift) ss << "Shift+";
    if (alt) ss << "Alt+";
    
    // For single letter keys, ImGui::GetKeyName might return "A" for ImGuiKey_A.
    // For other keys like ImGuiKey_LeftArrow, it's descriptive.
    ss << ImGui::GetKeyName(key);
    return ss.str();
}

std::string KeyCombination::ToConfigString() const {
    if (key == ImGuiKey_None) return ""; // Empty string for unbound
    std::stringstream ss;
    if (ctrl) ss << "C+";
    if (shift) ss << "S+";
    if (alt) ss << "A+";
    ss << static_cast<int>(key); // Store ImGuiKey as its integer value for robustness
    return ss.str();
}

KeyCombination KeyCombination::FromConfigString(const std::string& s) {
    if (s.empty()) return KeyCombination(ImGuiKey_None);

    KeyCombination kb;
    std::string remaining = s;
    size_t pos;

    // Check for modifiers
    if ((pos = remaining.find("C+")) == 0) {
        kb.ctrl = true;
        remaining = remaining.substr(2);
    }
    if ((pos = remaining.find("S+")) == 0) {
        kb.shift = true;
        remaining = remaining.substr(2);
    }
    if ((pos = remaining.find("A+")) == 0) {
        kb.alt = true;
        remaining = remaining.substr(2);
    }
    
    // The rest should be the integer value of ImGuiKey
    try {
        int keyCode = std::stoi(remaining);
        if (keyCode > ImGuiKey_NamedKey_BEGIN && keyCode < ImGuiKey_NamedKey_END) { // Basic validation for standard keys
            kb.key = static_cast<ImGuiKey>(keyCode);
        } else if (keyCode >= ImGuiKey_GamepadStart && keyCode <= ImGuiKey_GamepadR3) { // Corrected Gamepad key range, using GamepadR3 as an example endpoint for stick buttons
            // Add more specific checks here if needed for various gamepad buttons
            // For example, check if keyCode is one of the ImGuiKey_GamepadXXX values explicitly
            kb.key = static_cast<ImGuiKey>(keyCode);
        }
        // Add more specific validation if certain ImGuiKey ranges are problematic.
    } catch (const std::invalid_argument& ia) {
        // Failed to parse key, return unbound
        return KeyCombination(ImGuiKey_None);
    } catch (const std::out_of_range& oor) {
        return KeyCombination(ImGuiKey_None);
    }

    return kb;
} 