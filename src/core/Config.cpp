#include "Config.hpp"
#include <stdexcept>

Config::Config() {
    // Set some default values
    SetString("application.name", "PCB Viewer");
    SetInt("window.width", 1280);
    SetInt("window.height", 720);
    SetBool("ui.darkMode", true);
}

Config::~Config() = default;

void Config::SetString(const std::string& key, const std::string& value) {
    m_values[key] = value;
}

void Config::SetInt(const std::string& key, int value) {
    m_values[key] = value;
}

void Config::SetFloat(const std::string& key, float value) {
    m_values[key] = value;
}

void Config::SetBool(const std::string& key, bool value) {
    m_values[key] = value;
}

std::string Config::GetString(const std::string& key, const std::string& defaultValue) const {
    auto it = m_values.find(key);
    if (it != m_values.end()) {
        try {
            return std::get<std::string>(it->second);
        } catch (const std::bad_variant_access&) {
            // Value exists but is not a string
        }
    }
    return defaultValue;
}

int Config::GetInt(const std::string& key, int defaultValue) const {
    auto it = m_values.find(key);
    if (it != m_values.end()) {
        try {
            return std::get<int>(it->second);
        } catch (const std::bad_variant_access&) {
            // Value exists but is not an int
        }
    }
    return defaultValue;
}

float Config::GetFloat(const std::string& key, float defaultValue) const {
    auto it = m_values.find(key);
    if (it != m_values.end()) {
        try {
            return std::get<float>(it->second);
        } catch (const std::bad_variant_access&) {
            // Value exists but is not a float
        }
    }
    return defaultValue;
}

bool Config::GetBool(const std::string& key, bool defaultValue) const {
    auto it = m_values.find(key);
    if (it != m_values.end()) {
        try {
            return std::get<bool>(it->second);
        } catch (const std::bad_variant_access&) {
            // Value exists but is not a bool
        }
    }
    return defaultValue;
}

bool Config::HasKey(const std::string& key) const {
    return m_values.find(key) != m_values.end();
} 