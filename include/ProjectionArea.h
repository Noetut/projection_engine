#ifndef PROJECTION_AREA_H
#define PROJECTION_AREA_H

#include <windows.h>
#include <string>
#include <vector>

#include "Geometry.h"

// One calibrated projection area.
//
// There are deliberately no width/height/centre members: those are derived from
// the quad (see Geometry.h) so that the corners remain the single description of
// the geometry, both here and in config/pattern_config.json.
struct ProjectionArea {
    int         id;          // Stable identifier, persisted; NOT the vector index
    std::string name;
    std::string description;
    std::string type;        // "quad" (default) or "text"
    Quad        quad;
    COLORREF    color;
    bool        isVisible;

    // Text rendering properties
    std::string text;            // Text to display when type == "text" or text is set
    std::string fontFace;        // Font family (e.g. "Arial")
    int         fontSize;        // Font size in points (e.g. 32)
    COLORREF    textColor;       // Color of the text glyphs

    // Reserved for animation modules
    std::string imagePath;       // Optional image path / name to display instead of solid fill
    float opacity;
    bool  isBlinking;
    float blinkRate;
    float phase;

    ProjectionArea()
        : id(0)
        , type("quad")
        , quad(Quad::FromRect(0, 0, 100, 100))
        , color(RGB(255, 255, 255))
        , isVisible(true)
        , fontFace("Arial")
        , fontSize(32)
        , textColor(RGB(255, 255, 255))
        , opacity(1.0f)
        , isBlinking(false)
        , blinkRate(1.0f)
        , phase(0.0f)
    {
    }
};

// Compares only the fields that get persisted. Calibration uses this to decide
// whether the in-memory grid still matches the saved config, so the transient
// animation fields are intentionally excluded.
inline bool SameAsPersisted(const ProjectionArea& a, const ProjectionArea& b) {
    for (int i = 0; i < 4; ++i) {
        if (a.quad.corners[i].x != b.quad.corners[i].x) return false;
        if (a.quad.corners[i].y != b.quad.corners[i].y) return false;
    }
    return a.id == b.id &&
           a.name == b.name &&
           a.description == b.description &&
           a.type == b.type &&
           a.color == b.color &&
           a.isVisible == b.isVisible;
}

inline bool SameAsPersisted(const std::vector<ProjectionArea>& a,
                            const std::vector<ProjectionArea>& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (!SameAsPersisted(a[i], b[i])) return false;
    }
    return true;
}

#endif // PROJECTION_AREA_H
