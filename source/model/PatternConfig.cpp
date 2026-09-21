#include "PatternConfig.h"

#include <windows.h>

#include <set>

#include <nlohmann/json.hpp>

#include "util/StringUtil.h"

using ordered_json = nlohmann::ordered_json;

const char* PatternConfig::SCHEMA_VERSION = "2.0";

namespace {

std::wstring ExecutableDirectory() {
    wchar_t buffer[MAX_PATH] = { 0 };
    DWORD written = GetModuleFileNameW(NULL, buffer, MAX_PATH);
    if (written == 0 || written >= MAX_PATH) return std::wstring();

    std::wstring path(buffer);
    size_t slash = path.find_last_of(L"\\/");
    if (slash == std::wstring::npos) return std::wstring();

    return path.substr(0, slash);
}

// Reads a whole file through the Win32 API rather than std::ifstream: wide-path
// stream constructors are an MSVC extension and this project builds with MinGW.
bool ReadWholeFile(const std::wstring& path, std::string& out, std::string& error) {
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        error = "could not open file for reading (Win32 error " + std::to_string(GetLastError()) + ")";
        return false;
    }

    LARGE_INTEGER size = {};
    if (!GetFileSizeEx(file, &size)) {
        error = "could not query file size";
        CloseHandle(file);
        return false;
    }

    if (size.QuadPart > 16 * 1024 * 1024) {
        error = "config file is implausibly large";
        CloseHandle(file);
        return false;
    }

    out.resize(static_cast<size_t>(size.QuadPart));
    if (size.QuadPart > 0) {
        DWORD read = 0;
        if (!ReadFile(file, &out[0], static_cast<DWORD>(size.QuadPart), &read, NULL) ||
            read != static_cast<DWORD>(size.QuadPart)) {
            error = "could not read file contents";
            CloseHandle(file);
            return false;
        }
    }

    CloseHandle(file);
    return true;
}

bool WriteWholeFile(const std::wstring& path, const std::string& content, std::string& error) {
    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, NULL,
                              CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        error = "could not open file for writing (Win32 error " + std::to_string(GetLastError()) + ")";
        return false;
    }

    if (!content.empty()) {
        DWORD written = 0;
        if (!WriteFile(file, content.data(), static_cast<DWORD>(content.size()), &written, NULL) ||
            written != static_cast<DWORD>(content.size())) {
            error = "could not write file contents";
            CloseHandle(file);
            return false;
        }
    }

    FlushFileBuffers(file);
    CloseHandle(file);
    return true;
}

COLORREF ParseColor(const ordered_json& node) {
    if (node.is_array() && node.size() == 3) {
        int r = node[0].get<int>();
        int g = node[1].get<int>();
        int b = node[2].get<int>();
        return RGB(static_cast<BYTE>(r), static_cast<BYTE>(g), static_cast<BYTE>(b));
    }
    return RGB(255, 255, 255);
}

// Accepts both schemas. v2 stores "corners"; v1.x stores x/y/width/height.
bool ParseArea(const ordered_json& node, ProjectionArea& out) {
    if (node.contains("corners") && node["corners"].is_array() && node["corners"].size() == 4) {
        for (int i = 0; i < 4; ++i) {
            const auto& corner = node["corners"][i];
            if (!corner.contains("x") || !corner.contains("y")) return false;
            out.quad.corners[i].x = corner["x"].get<int>();
            out.quad.corners[i].y = corner["y"].get<int>();
        }
    } else if (node.contains("x") && node.contains("y") &&
               node.contains("width") && node.contains("height")) {
        out.quad = Quad::FromRect(node["x"].get<int>(),
                                  node["y"].get<int>(),
                                  node["width"].get<int>(),
                                  node["height"].get<int>());
    } else {
        return false;
    }

    out.id          = node.value("id", 0);
    out.type        = node.value("type", "quad");
    out.name        = node.value("name", std::string());
    out.description = node.value("description", std::string());
    out.isVisible   = node.value("visible", true);
    out.color       = node.contains("color") ? ParseColor(node["color"]) : RGB(255, 255, 255);
    return true;
}

} // namespace

std::wstring PatternConfig::ResolvePath(const std::wstring& explicitPath) {
    if (!explicitPath.empty()) return explicitPath;

    const std::wstring exeDir = ExecutableDirectory();
    const std::wstring relative = L"config\\pattern_config.json";

    // The executable normally lives in build/ (or build/<Config>/), so walk up.
    std::vector<std::wstring> candidates;
    if (!exeDir.empty()) {
        candidates.push_back(exeDir + L"\\" + relative);
        candidates.push_back(exeDir + L"\\..\\" + relative);
        candidates.push_back(exeDir + L"\\..\\..\\" + relative);
    }
    candidates.push_back(relative); // Last resort: current working directory

    for (const auto& candidate : candidates) {
        if (Exists(candidate)) return candidate;
    }

    // Nothing found: point at the canonical repository location so that saving
    // from calibration mode creates the file in the right place.
    if (!exeDir.empty()) return exeDir + L"\\..\\" + relative;
    return relative;
}

bool PatternConfig::Exists(const std::wstring& path) {
    DWORD attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES &&
           (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

bool PatternConfig::Load(const std::wstring& path, PatternConfigData& out, std::string& error) {
    std::string raw;
    if (!ReadWholeFile(path, raw, error)) return false;

    ordered_json root;
    try {
        root = ordered_json::parse(raw);
    } catch (const std::exception& e) {
        error = std::string("JSON parse error: ") + e.what();
        return false;
    }

    try {
        if (root.contains("canvas")) {
            out.canvasWidth  = root["canvas"].value("width",  1920);
            out.canvasHeight = root["canvas"].value("height", 1080);
        }
        out.referenceImage = root.value("reference_image", std::string("Patrón.png"));

        // v2 calls the collection "areas"; v1.x called it "boxes".
        const ordered_json* collection = nullptr;
        if (root.contains("areas") && root["areas"].is_array()) {
            collection = &root["areas"];
        } else if (root.contains("boxes") && root["boxes"].is_array()) {
            collection = &root["boxes"];
        } else {
            error = "no \"areas\" or \"boxes\" array found";
            return false;
        }

        out.areas.clear();
        out.areas.reserve(collection->size());

        std::set<int> usedIds;
        int highestId = -1;

        for (const auto& node : *collection) {
            ProjectionArea area;
            if (!ParseArea(node, area)) {
                error = "an entry is missing its geometry (needs \"corners\" or x/y/width/height)";
                return false;
            }

            // Ids must stay unique: duplicates in a hand-edited file would make
            // id-based lookups ambiguous.
            if (!usedIds.insert(area.id).second) {
                area.id = highestId + 1;
                usedIds.insert(area.id);
            }
            if (area.id > highestId) highestId = area.id;

            if (area.name.empty()) {
                area.name = "area_" + std::to_string(area.id);
            }

            out.areas.push_back(area);
        }
    } catch (const std::exception& e) {
        error = std::string("JSON structure error: ") + e.what();
        return false;
    }

    return true;
}

bool PatternConfig::Save(const std::wstring& path, const PatternConfigData& data, std::string& error) {
    ordered_json root;
    root["version"] = SCHEMA_VERSION;
    root["reference_image"] = data.referenceImage;
    root["canvas"] = ordered_json{ { "width", data.canvasWidth }, { "height", data.canvasHeight } };

    // Derived values (width, height, centre, total count) are deliberately not
    // written: the corners are the only description of the geometry.
    ordered_json areas = ordered_json::array();
    for (const auto& area : data.areas) {
        ordered_json corners = ordered_json::array();
        for (int i = 0; i < 4; ++i) {
            corners.push_back(ordered_json{ { "x", area.quad.corners[i].x },
                                            { "y", area.quad.corners[i].y } });
        }

        ordered_json node;
        node["id"] = area.id;
        if (area.type == "text") {
            node["type"] = "text";
        }
        node["name"] = area.name;
        node["description"] = area.description;
        node["corners"] = corners;
        node["color"] = ordered_json::array({ static_cast<int>(GetRValue(area.color)),
                                              static_cast<int>(GetGValue(area.color)),
                                              static_cast<int>(GetBValue(area.color)) });
        node["visible"] = area.isVisible;
        areas.push_back(node);
    }
    root["areas"] = areas;

    std::string serialized;
    try {
        // The replace error handler keeps a hand-edited file with a stray
        // non-UTF-8 byte from turning a save into a lost calibration.
        serialized = root.dump(4, ' ', false, nlohmann::detail::error_handler_t::replace);
        serialized += "\n";
    } catch (const std::exception& e) {
        error = std::string("JSON serialisation error: ") + e.what();
        return false;
    }

    const std::wstring tempPath   = path + L".tmp";
    const std::wstring backupPath = path + L".bak";

    if (!WriteWholeFile(tempPath, serialized, error)) return false;

    if (Exists(path) && !CopyFileW(path.c_str(), backupPath.c_str(), FALSE)) {
        error = "could not create .bak backup (Win32 error " + std::to_string(GetLastError()) + ")";
        DeleteFileW(tempPath.c_str());
        return false;
    }

    if (!MoveFileExW(tempPath.c_str(), path.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        error = "could not replace config file (Win32 error " + std::to_string(GetLastError()) + ")";
        DeleteFileW(tempPath.c_str());
        return false;
    }

    return true;
}
