#include "Config.hpp"

#include <algorithm>  // For std::transform (for bool parsing)
#include <fstream>    // For file operations
#include <iostream>   // For error logging (optional)
#include <sstream>    // For string stream operations
#include <stdexcept>

#include "utils/StringUtils.hpp"

Config::Config()
{
    // Set some default values (these will be used if config file is missing or keys are absent)
    SetString("application.name", "PCB Viewer");
    SetInt("window.width", 1280);
    SetInt("window.height", 720);
    SetBool("ui.darkMode", true);
    // Default keybinds are initialized in ControlSettings,
    // Config will only store them if they are modified or explicitly saved.
}

Config::~Config() = default;

void Config::SetString(const std::string& key, const std::string& value)
{
    m_values[key] = value;
}

void Config::SetInt(const std::string& key, int value)
{
    m_values[key] = value;
}

void Config::SetFloat(const std::string& key, float value)
{
    m_values[key] = value;
}

void Config::SetBool(const std::string& key, bool value)
{
    m_values[key] = value;
}

std::string Config::GetString(const std::string& key, const std::string& defaultValue) const
{
    auto it = m_values.find(key);
    if (it != m_values.end()) {
        try {
            if (std::holds_alternative<std::string>(it->second)) {
                return std::get<std::string>(it->second);
            }
            return std::visit(
                [](const auto& val_in) -> std::string {
                    std::stringstream ss;
                    if constexpr (std::is_same_v<std::decay_t<decltype(val_in)>, bool>) {
                        ss << (val_in ? "true" : "false");
                    } else {
                        ss << val_in;
                    }
                    return ss.str();
                },
                it->second);
        } catch (const std::bad_variant_access&) {
        }
    }
    return defaultValue;
}

int Config::GetInt(const std::string& key, int defaultValue) const
{
    auto it = m_values.find(key);
    if (it != m_values.end()) {
        try {
            if (std::holds_alternative<int>(it->second))
                return std::get<int>(it->second);
            if (std::holds_alternative<std::string>(it->second)) {
                return std::stoi(std::get<std::string>(it->second));
            }
        } catch (const std::exception&) {
        }
    }
    return defaultValue;
}

float Config::GetFloat(const std::string& key, float defaultValue) const
{
    auto it = m_values.find(key);
    if (it != m_values.end()) {
        try {
            if (std::holds_alternative<float>(it->second))
                return std::get<float>(it->second);
            if (std::holds_alternative<std::string>(it->second)) {
                return std::stof(std::get<std::string>(it->second));
            }
            if (std::holds_alternative<int>(it->second)) {  // Promote int to float
                return static_cast<float>(std::get<int>(it->second));
            }
        } catch (const std::exception&) {
        }
    }
    return defaultValue;
}

bool Config::GetBool(const std::string& key, bool defaultValue) const
{
    auto it = m_values.find(key);
    if (it != m_values.end()) {
        try {
            if (std::holds_alternative<bool>(it->second))
                return std::get<bool>(it->second);
            if (std::holds_alternative<std::string>(it->second)) {
                std::string valStr = std::get<std::string>(it->second);
                std::transform(valStr.begin(), valStr.end(), valStr.begin(), ::tolower);
                if (valStr == "true" || valStr == "1")
                    return true;
                if (valStr == "false" || valStr == "0")
                    return false;
            }
        } catch (const std::exception&) {
        }
    }
    return defaultValue;
}

bool Config::HasKey(const std::string& key) const
{
    return m_values.find(key) != m_values.end();
}

// --- File I/O Implementation ---

bool Config::SaveToFile(const std::string& filename) const
{
    std::ofstream configFile(filename);
    if (!configFile.is_open()) {
        // std::cerr << "Error: Could not open config file for writing: " << filename << std::endl;
        return false;
    }

    const std::string IMGUI_INI_DATA_KEY_CONST = "imgui.ini_data";
    const std::string FILE_DIALOG_BOOKMARKS_KEY_CONST = "file_dialog.bookmarks";

    for (const auto& pair : m_values) {
        configFile << pair.first << "=";
        std::visit(
            [&configFile, &key = pair.first, &IMGUI_INI_DATA_KEY_CONST, &FILE_DIALOG_BOOKMARKS_KEY_CONST](const auto& value) {
                if constexpr (std::is_same_v<std::decay_t<decltype(value)>, std::string>) {
                    if (key == IMGUI_INI_DATA_KEY_CONST) {
                        configFile << string_utils::EscapeNewlines(value);
                    } else if (key == FILE_DIALOG_BOOKMARKS_KEY_CONST) {
                        configFile << string_utils::EscapeHashes(value);
                    } else {
                        configFile << value;
                    }
                } else if constexpr (std::is_same_v<std::decay_t<decltype(value)>, bool>) {
                    configFile << (value ? "true" : "false");
                } else {
                    configFile << value;
                }
            },
            pair.second);
        configFile << "\n";
    }
    configFile.close();
    return true;
}

bool Config::LoadFromFile(const std::string& filename)
{
    m_values.clear();
    std::ifstream configFile(filename);
    if (!configFile.is_open()) {
        return false;
    }
    const std::string IMGUI_INI_DATA_KEY_CONST = "imgui.ini_data";
    const std::string FILE_DIALOG_BOOKMARKS_KEY_CONST = "file_dialog.bookmarks";
    std::string line;
    while (std::getline(configFile, line)) {
        std::string key, valueStr;
        size_t delimiterPos = line.find('=');
        if (delimiterPos != std::string::npos) {
            key = string_utils::Trim(line.substr(0, delimiterPos));
            valueStr = string_utils::Trim(line.substr(delimiterPos + 1));

            if (key.empty() || key[0] == '#' || key[0] == ';') {
                continue;
            }

            if (key == IMGUI_INI_DATA_KEY_CONST) {
                SetString(key, valueStr);
                continue;
            }

            if (key == FILE_DIALOG_BOOKMARKS_KEY_CONST) {
                SetString(key, string_utils::UnescapeHashes(valueStr));
                continue;
            }

            std::string lowerValueStr = valueStr;
            std::transform(lowerValueStr.begin(), lowerValueStr.end(), lowerValueStr.begin(), ::tolower);

            if (lowerValueStr == "true") {
                SetBool(key, true);
            } else if (lowerValueStr == "false") {
                SetBool(key, false);
            } else {
                try {
                    size_t processedChars = 0;
                    int intVal = std::stoi(valueStr, &processedChars);
                    if (processedChars == valueStr.length()) {
                        SetInt(key, intVal);
                        continue;
                    }
                } catch (const std::exception&) {
                }
                try {
                    size_t processedChars = 0;
                    float floatVal = std::stof(valueStr, &processedChars);
                    if (processedChars == valueStr.length()) {
                        SetFloat(key, floatVal);
                        continue;
                    }
                } catch (const std::exception&) {
                }
                SetString(key, valueStr);
            }
        }
    }
    configFile.close();
    return true;
}
