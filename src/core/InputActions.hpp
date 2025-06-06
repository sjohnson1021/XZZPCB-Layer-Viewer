#pragma once

#include <cstdint>
#include <map>  // For storing keybinds
#include <string>

#include "imgui.h"  // For ImGuiKey and other ImGui types

// Enum for all bindable input actions
enum class InputAction : uint8_t {
    kPanUp,
    kPanLeft,
    kPanDown,
    kPanRight,
    kRotateLeft,
    kRotateRight,
    kZoomIn,
    kZoomOut,
    kResetView,
    kFlipBoard,
    // Add more actions as needed in the future

    kCount  // Special value to get the number of actions, keep it last
};

// Helper to get a user-friendly string name for an action
const char* InputActionToString(InputAction action);

// Structure to define a keybinding (key + modifiers)
struct KeyCombination {
    ImGuiKey key = ImGuiKey_None;
    bool ctrl = false;
    bool shift = false;
    bool alt = false;

    KeyCombination(ImGuiKey k = ImGuiKey_None, bool c = false, bool s = false, bool a = false) : key(k), ctrl(c), shift(s), alt(a) {}

    [[nodiscard]] bool IsBound() const { return key != ImGuiKey_None; }
    [[nodiscard]] std::string ToString() const;                    // For displaying the keybind
    [[nodiscard]] std::string ToConfigString() const;              // For saving to config file
    static KeyCombination FromConfigString(const std::string& s);  // For loading from config

    bool operator==(const KeyCombination& other) const { return key == other.key && ctrl == other.ctrl && shift == other.shift && alt == other.alt; }
    bool operator!=(const KeyCombination& other) const { return !(*this == other); }
};

// Type alias for convenience
using KeybindMap = std::map<InputAction, KeyCombination>;
