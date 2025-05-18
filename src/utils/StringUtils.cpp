#include "StringUtils.hpp"
#include <algorithm> // For std::remove_if for Trim
#include <cctype>    // For std::isspace for Trim

namespace StringUtils {

std::string ReplaceAll(std::string str, const std::string& from, const std::string& to) {
    size_t start_pos = 0;
    while((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length(); // Handles cases where 'to' is a substring of 'from'
    }
    return str;
}

std::string EscapeNewlines(const std::string& input) {
    return ReplaceAll(input, "\n", "\\n");
}

std::string UnescapeNewlines(const std::string& input) {
    return ReplaceAll(input, "\\n", "\n");
}

// Function to trim leading whitespace
static std::string ltrim(std::string s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    }));
    return s;
}

// Function to trim trailing whitespace
static std::string rtrim(std::string s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(), s.end());
    return s;
}

// Function to trim both leading and trailing whitespace
std::string Trim(const std::string& str) {
    return ltrim(rtrim(str));
}

} // namespace StringUtils 