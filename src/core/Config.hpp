#pragma once

#include <string>
#include <unordered_map>
#include <variant>

class Config {
public:
    Config();
    ~Config();

    // Basic settings
    void SetString(const std::string& key, const std::string& value);
    void SetInt(const std::string& key, int value);
    void SetFloat(const std::string& key, float value);
    void SetBool(const std::string& key, bool value);

    std::string GetString(const std::string& key, const std::string& defaultValue = "") const;
    int GetInt(const std::string& key, int defaultValue = 0) const;
    float GetFloat(const std::string& key, float defaultValue = 0.0f) const;
    bool GetBool(const std::string& key, bool defaultValue = false) const;

    bool HasKey(const std::string& key) const;

private:
    using ConfigValue = std::variant<std::string, int, float, bool>;
    std::unordered_map<std::string, ConfigValue> m_values;
}; 