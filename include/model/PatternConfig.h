#ifndef PATTERN_CONFIG_H
#define PATTERN_CONFIG_H

#include <string>
#include <vector>

#include "ProjectionArea.h"

// Everything persisted in config/pattern_config.json.
struct PatternConfigData {
    int         canvasWidth  = 1920;
    int         canvasHeight = 1080;
    std::string referenceImage = "Patrón.png";
    std::vector<ProjectionArea> areas;
};

// config/pattern_config.json is the single source of truth for the projection
// geometry: it is read at start-up and rewritten by calibration mode. No
// coordinates are compiled into the binary.
class PatternConfig {
public:
    static const char* SCHEMA_VERSION; // "2.0"

    // Resolves the config path. If explicitPath is non-empty it wins verbatim;
    // otherwise the candidates below the executable directory are probed, and
    // the canonical repository location is returned when none exists yet (so
    // that a first Save() still lands somewhere sensible).
    static std::wstring ResolvePath(const std::wstring& explicitPath);

    static bool Exists(const std::wstring& path);

    // Reads either schema v2 ("areas" with "corners") or the legacy v1.x schema
    // ("boxes" with x/y/width/height), converting the latter to corners.
    static bool Load(const std::wstring& path, PatternConfigData& out, std::string& error);

    // Always writes schema v2. Writes to a temporary file, keeps a .bak copy of
    // the previous config and then replaces it, so an interrupted save cannot
    // destroy a calibration.
    static bool Save(const std::wstring& path, const PatternConfigData& data, std::string& error);
};

#endif // PATTERN_CONFIG_H
