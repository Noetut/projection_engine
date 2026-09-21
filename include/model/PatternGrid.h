#ifndef PATTERN_GRID_H
#define PATTERN_GRID_H

#include <string>
#include <vector>

#include "ProjectionArea.h"

// Holds the runtime collection of projection areas.
//
// Note on addressing: every method below takes a vector INDEX, not the
// persisted ProjectionArea::id. The two coincide in a freshly loaded config but
// diverge as soon as areas are created or deleted during calibration, so the
// id-based lookups are kept separate and explicit.
class PatternGrid {
public:
    PatternGrid();
    ~PatternGrid();

    // --- Bulk state -------------------------------------------------------
    void Clear();
    void SetAreas(const std::vector<ProjectionArea>& areas);

    // --- Visibility -------------------------------------------------------
    void SetAreaVisible(int index, bool visible);
    void ToggleArea(int index);
    void SetAllVisible(bool visible);
    bool IsAreaVisible(int index) const;

    // --- Colour & Image --------------------------------------------------
    void SetAreaColor(int index, COLORREF color);
    void SetAllColor(COLORREF color);
    void SetAreaImage(int index, const std::string& imagePath);
    void ClearAreaImage(int index);
    void ClearAllImages();

    // --- Text -------------------------------------------------------------
    void SetAreaText(int index, const std::string& text, const std::string& fontFace = "Arial", int fontSize = 32, COLORREF color = RGB(255, 255, 255));
    void ClearAreaText(int index);
    void ClearAllTexts();

    // --- Lifecycle (calibration) ------------------------------------------
    // Appends a default area centred on the canvas and returns its index.
    int  CreateArea(int canvasWidth, int canvasHeight);
    // Appends a default text area centred on the canvas and returns its index.
    int  CreateTextArea(int canvasWidth, int canvasHeight);
    // Appends a copy of an existing area offset by a few pixels; returns the
    // new index, or -1 if the source index is invalid.
    int  DuplicateArea(int index);
    bool RemoveArea(int index);

    // --- Lookup -----------------------------------------------------------
    int NextFreeId() const;
    int FindIndexById(int id) const;
    int FindIndexByName(const std::string& name) const;

    // --- Accessors --------------------------------------------------------
    const std::vector<ProjectionArea>& GetAreas() const { return m_areas; }
    std::vector<ProjectionArea>&       GetAreas()       { return m_areas; }
    size_t GetCount() const { return m_areas.size(); }

    ProjectionArea*       GetArea(int index);
    const ProjectionArea* GetArea(int index) const;

private:
    bool IsValidIndex(int index) const {
        return index >= 0 && index < static_cast<int>(m_areas.size());
    }

    std::vector<ProjectionArea> m_areas;
};

#endif // PATTERN_GRID_H
