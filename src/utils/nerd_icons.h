#pragma once

#include <string>

namespace NerdIcons {
    // Helper function to convert Unicode codepoint to UTF-8
    // This is a compile-time approach for the icons you specified
  // Use standard Font Awesome icons instead (all within supported range)
    constexpr const char* kHome = "\xef\x80\x95";     // U+F015 (nf-fa-home)
    constexpr const char* kDesktop = "\xef\x80\x9c";         // U+F01C (nf-fa-desktop)
    constexpr const char* kDocuments = "\xef\x81\xbb";       // U+F07B (nf-fa-folder)
    constexpr const char* kDownloads = "\xef\x81\x99";       // U+F019 (nf-fa-download)
    constexpr const char* kCurrentDir = "\xef\x81\xbc";     // U+F07C (nf-fa-folder-open)
    constexpr const char* kBookmarks = "\xef\x80\x86";        // U+F006 (nf-fa-bookmark)
    
    // Your requested icons (these work fine)
    constexpr const char* MINUS = "\xef\x81\xa8";           // U+F068 (nf-fa-minus)
    constexpr const char* PLUS = "\xef\x81\xa7";            // U+F067 (nf-fa-plus)
    constexpr const char* EDIT = "\xef\x81\x84";            // U+F044 (nf-fa-edit)
    constexpr const char* DETAILS = "\xef\x80\xa2";         // U+F022 (nf-fa-details)
    // Helper function to create bookmarks with icon prefix
    inline std::string WithBookmarkIcon(const std::string& text) {
        return std::string(kBookmarks) + " " + text;
    }
}