#pragma once

#include <string>

namespace StringUtils {

// Replaces all occurrences of 'from' with 'to' in 'str'.
std::string ReplaceAll(std::string str, const std::string& from, const std::string& to);

// Escapes newline characters (\n) with literal \\n.
std::string EscapeNewlines(const std::string& input);

// Unescapes literal \\n back to newline characters (\n).
std::string UnescapeNewlines(const std::string& input);

// Trims leading and trailing whitespace from a string.
std::string Trim(const std::string& str);

} // namespace StringUtils 