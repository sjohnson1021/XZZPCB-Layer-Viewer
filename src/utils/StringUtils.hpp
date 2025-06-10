#pragma once

#include <string>

namespace string_utils
{

// Replaces all occurrences of 'src' with 'dst' in 'str'.
std::string ReplaceAll(std::string str, const std::string& src, const std::string& dst);

// Escapes newline characters (\n) with literal \\n.
std::string EscapeNewlines(const std::string& input);

// Unescapes literal \\n back to newline characters (\n).
std::string UnescapeNewlines(const std::string& input);

// Escapes hash characters (#) with literal \\#.
std::string EscapeHashes(const std::string& input);

// Unescapes literal \\# back to hash characters (#).
std::string UnescapeHashes(const std::string& input);

// Trims trailing whitespace from a string.
std::string Rtrim(std::string str);

// Trims leading whitespace from a string.
std::string Ltrim(std::string str);

// Trims leading and trailing whitespace from a string.
std::string Trim(std::string str);

}  // namespace string_utils
