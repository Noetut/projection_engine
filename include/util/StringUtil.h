#ifndef STRING_UTIL_H
#define STRING_UTIL_H

#include <windows.h>
#include <string>

// Narrow (UTF-8) to wide conversion. Area names and file paths coming from the
// JSON config are UTF-8; the Win32 text APIs used by the HUD need UTF-16.
inline std::wstring Utf8ToWide(const std::string& text) {
    if (text.empty()) return std::wstring();

    int needed = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), NULL, 0);
    if (needed <= 0) return std::wstring();

    std::wstring result(static_cast<size_t>(needed), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), &result[0], needed);
    return result;
}

inline std::string WideToUtf8(const std::wstring& text) {
    if (text.empty()) return std::string();

    int needed = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), NULL, 0, NULL, NULL);
    if (needed <= 0) return std::string();

    std::string result(static_cast<size_t>(needed), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), &result[0], needed, NULL, NULL);
    return result;
}

// argv strings arrive in the process ANSI code page.
inline std::wstring AnsiToWide(const char* text) {
    if (!text || !*text) return std::wstring();

    int needed = MultiByteToWideChar(CP_ACP, 0, text, -1, NULL, 0);
    if (needed <= 1) return std::wstring();

    std::wstring result(static_cast<size_t>(needed - 1), L'\0');
    MultiByteToWideChar(CP_ACP, 0, text, -1, &result[0], needed - 1);
    return result;
}

inline std::wstring AnsiToWide(const std::string& text) {
    return AnsiToWide(text.c_str());
}

#endif // STRING_UTIL_H
