#include "core/ControlSettings.hpp"

#include <string>  // For std::to_string

#include "core/Config.hpp"  // Required for Load/Save methods

// ControlSettings::ControlSettings() {
//     // Initialize default keybinds if necessary
//     // m_keybinds[InputAction::OpenFile] = "Ctrl+O";
// }

ControlSettings::ControlSettings()
{
    InitializeDefaultKeybinds();
    // Later, we will add loading from config here, potentially overriding defaults.
}

void ControlSettings::InitializeDefaultKeybinds()
{
    ControlSettings::m_keybinds.clear();
    // W, Up arrow: pan up
    ControlSettings::m_keybinds[InputAction::kPanUp] = KeyCombination(ImGuiKey_W);
    // A, Left arrow: pan left
    ControlSettings::m_keybinds[InputAction::kPanLeft] = KeyCombination(ImGuiKey_A);
    // S, Down arrow: pan down
    ControlSettings::m_keybinds[InputAction::kPanDown] = KeyCombination(ImGuiKey_S);
    // D, Right arrow: pan right
    ControlSettings::m_keybinds[InputAction::kPanRight] = KeyCombination(ImGuiKey_D);
    // Q: rotate left
    ControlSettings::m_keybinds[InputAction::kRotateLeft] = KeyCombination(ImGuiKey_Q);
    // E: rotate right
    ControlSettings::m_keybinds[InputAction::kRotateRight] = KeyCombination(ImGuiKey_E);
    // R: reset view
    ControlSettings::m_keybinds[InputAction::kResetView] = KeyCombination(ImGuiKey_R);
    // F: flip board
    ControlSettings::m_keybinds[InputAction::kFlipBoard] = KeyCombination(ImGuiKey_F);
    // +: zoom in
    ControlSettings::m_keybinds[InputAction::kZoomIn] = KeyCombination(ImGuiKey_Equal);  // Equal sign is often plus without shift
    // -: zoom out
    ControlSettings::m_keybinds[InputAction::kZoomOut] = KeyCombination(ImGuiKey_Minus);

    // It's a good idea to also map arrow keys and keypad +/- if desired as secondary defaults,
    // but the system should allow users to set these. For now, one primary default.
    // Example for secondary:
    // if (m_keybinds.find(InputAction::PanUp) == m_keybinds.end() || !m_keybinds[InputAction::PanUp].IsBound()) {
    //     m_keybinds[InputAction::PanUpSecondary] = KeyCombination(ImGuiKey_UpArrow)
    // }
    // This example implies needing secondary actions or a list of keybinds per action.
    // For simplicity now, one keybind per action.
}

KeyCombination ControlSettings::GetKeybind(InputAction action) const
{
    auto it = ControlSettings::m_keybinds.find(action);
    if (it != ControlSettings::m_keybinds.end()) {
        return it->second;
    }
    return KeyCombination(ImGuiKey_None);  // Return unbound if not found
}

void ControlSettings::SetKeybind(InputAction action, KeyCombination keyCombination)
{
    // Optional: Before setting, check if this keyCombination is already bound to another action.
    // If so, either unbind the other action or prevent this assignment.
    // For now, allow overwriting/duplicate bindings; UI can warn user.
    ControlSettings::m_keybinds[action] = keyCombination;
}

void ControlSettings::ResetKeybindsToDefault()
{
    InitializeDefaultKeybinds();
}

// --- Config Interaction ---
static std::string GetConfigKeyForAction(InputAction action)
{
    return "keybind." + std::string(InputActionToString(action));
}

void ControlSettings::LoadKeybindsFromConfig(const Config& config)
{
    // Initialize with defaults first, so if some are missing from config, they are still set.
    InitializeDefaultKeybinds();

    for (int i = 0; i < static_cast<int>(InputAction::kCount); ++i) {
        InputAction action = static_cast<InputAction>(i);
        std::string configKey = GetConfigKeyForAction(action);
        if (config.HasKey(configKey)) {
            std::string kbString = config.GetString(configKey);
            KeyCombination kb = KeyCombination::FromConfigString(kbString);
            if (kb.IsBound()) {  // Only set if successfully parsed to a bound key
                ControlSettings::m_keybinds[action] = kb;
            }
        }
    }
}

void ControlSettings::SaveKeybindsToConfig(Config& config)
{
    for (const auto& pair : ControlSettings::m_keybinds) {
        InputAction action = pair.first;
        const KeyCombination& kb = pair.second;
        std::string configKey = GetConfigKeyForAction(action);
        config.SetString(configKey, kb.ToConfigString());
    }
}

// Implement load/save methods here if added to the header
