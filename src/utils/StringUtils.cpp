#include "StringUtils.hpp"

#include <algorithm>  // For std::remove_if for Trim
#include <cctype>     // For std::isspace for Trim
#include <cstddef>
#include <string>
#include <utility>


namespace string_utils
{
// Function to trim leading whitespace
std::string Ltrim(std::string str)
{
    str.erase(str.begin(), std::find_if(str.begin(), str.end(), [](unsigned char chr) { return !std::isspace(chr); }));
    return str;
}

// Function to trim trailing whitespace
std::string Rtrim(std::string str)
{
    str.erase(std::find_if(str.rbegin(), str.rend(), [](unsigned char chr) { return !std::isspace(chr); }).base(), str.end());
    return str;
}


std::string ReplaceAll(std::string str, const std::string& src, const std::string& dst)
{
    size_t start_pos = 0;
    while ((start_pos = str.find(src, start_pos)) != std::string::npos) {
        str.replace(start_pos, src.length(), dst);
        start_pos += dst.length();  // Handles cases where 'dst' is a substring of 'src'
    }
    return str;
}

std::string EscapeNewlines(const std::string& input)
{
    return ReplaceAll(input, "\n", "\\n");
}

std::string UnescapeNewlines(const std::string& input)
{
    return ReplaceAll(input, "\\n", "\n");
}


// Function to trim both leading and trailing whitespace
std::string Trim(std::string str)
{
    return Ltrim(Rtrim(std::move(str)));
}

}  // namespace string_utils
