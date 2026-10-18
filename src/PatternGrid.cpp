#include "PatternGrid.h"

namespace {

// Geometry of an area created with [N] in calibration mode.
const int kNewAreaWidth  = 200;
const int kNewAreaHeight = 150;

// Geometry of a text area created with [T] in calibration mode.
const int kNewTextAreaWidth  = 600;
const int kNewTextAreaHeight = 120;

// Offset applied to a copy made with [D], so it does not hide under the source.
const int kDuplicateOffset = 20;

} // namespace

PatternGrid::PatternGrid() {
}

PatternGrid::~PatternGrid() {
}

void PatternGrid::Clear() {
    m_areas.clear();
}

void PatternGrid::SetAreas(const std::vector<ProjectionArea>& areas) {
    m_areas = areas;
}

void PatternGrid::SetAreaVisible(int index, bool visible) {
    if (IsValidIndex(index)) {
        m_areas[index].isVisible = visible;
    }
}

void PatternGrid::ToggleArea(int index) {
    if (IsValidIndex(index)) {
        m_areas[index].isVisible = !m_areas[index].isVisible;
    }
}

void PatternGrid::SetAllVisible(bool visible) {
    for (auto& area : m_areas) {
        area.isVisible = visible;
    }
}

bool PatternGrid::IsAreaVisible(int index) const {
    return IsValidIndex(index) ? m_areas[index].isVisible : false;
}

void PatternGrid::SetAreaColor(int index, COLORREF color) {
    if (IsValidIndex(index)) {
        m_areas[index].color = color;
    }
}

void PatternGrid::SetAllColor(COLORREF color) {
    for (auto& area : m_areas) {
        area.color = color;
    }
}

void PatternGrid::SetAreaImage(int index, const std::string& imagePath) {
    if (IsValidIndex(index)) {
        m_areas[index].imagePath = imagePath;
    }
}

void PatternGrid::ClearAreaImage(int index) {
    if (IsValidIndex(index)) {
        m_areas[index].imagePath.clear();
    }
}

void PatternGrid::ClearAllImages() {
    for (auto& area : m_areas) {
        area.imagePath.clear();
    }
}

void PatternGrid::SetAreaText(int index, const std::string& text, const std::string& fontFace, int fontSize, COLORREF color) {
    if (IsValidIndex(index)) {
        m_areas[index].text = text;
        m_areas[index].fontFace = fontFace.empty() ? "Arial" : fontFace;
        m_areas[index].fontSize = fontSize > 0 ? fontSize : 32;
        m_areas[index].textColor = color;
    }
}

void PatternGrid::ClearAreaText(int index) {
    if (IsValidIndex(index)) {
        m_areas[index].text.clear();
    }
}

void PatternGrid::ClearAllTexts() {
    for (auto& area : m_areas) {
        area.text.clear();
    }
}

int PatternGrid::CreateArea(int canvasWidth, int canvasHeight) {
    ProjectionArea area;
    area.id = NextFreeId();
    area.type = "quad";
    area.name = "area_" + std::to_string(area.id);
    area.description = "Created in calibration mode";
    area.quad = Quad::FromRect((canvasWidth  - kNewAreaWidth)  / 2,
                               (canvasHeight - kNewAreaHeight) / 2,
                               kNewAreaWidth,
                               kNewAreaHeight);
    area.color = RGB(255, 255, 255);
    area.isVisible = true;

    m_areas.push_back(area);
    return static_cast<int>(m_areas.size()) - 1;
}

int PatternGrid::CreateTextArea(int canvasWidth, int canvasHeight) {
    ProjectionArea area;
    area.id = NextFreeId();
    area.type = "text";
    area.name = "text_box_" + std::to_string(area.id);
    area.description = "Text projection box created in calibration";
    area.quad = Quad::FromRect((canvasWidth  - kNewTextAreaWidth)  / 2,
                               (canvasHeight - kNewTextAreaHeight) / 2,
                               kNewTextAreaWidth,
                               kNewTextAreaHeight);
    area.color = RGB(255, 255, 255);
    area.textColor = RGB(255, 255, 255);
    area.fontFace = "Arial";
    area.fontSize = 32;
    area.isVisible = true;

    m_areas.push_back(area);
    return static_cast<int>(m_areas.size()) - 1;
}

int PatternGrid::DuplicateArea(int index) {
    if (!IsValidIndex(index)) return -1;

    ProjectionArea copy = m_areas[index];
    copy.id = NextFreeId();
    copy.name = "area_" + std::to_string(copy.id);
    copy.description = "Duplicated from " + m_areas[index].name;
    copy.quad.Translate(kDuplicateOffset, kDuplicateOffset);

    m_areas.push_back(copy);
    return static_cast<int>(m_areas.size()) - 1;
}

bool PatternGrid::RemoveArea(int index) {
    if (!IsValidIndex(index)) return false;

    m_areas.erase(m_areas.begin() + index);
    return true;
}

int PatternGrid::NextFreeId() const {
    int maxId = -1;
    for (const auto& area : m_areas) {
        if (area.id > maxId) maxId = area.id;
    }
    return maxId + 1;
}

int PatternGrid::FindIndexById(int id) const {
    for (size_t i = 0; i < m_areas.size(); ++i) {
        if (m_areas[i].id == id) return static_cast<int>(i);
    }
    return -1;
}

int PatternGrid::FindIndexByName(const std::string& name) const {
    for (size_t i = 0; i < m_areas.size(); ++i) {
        if (m_areas[i].name == name) return static_cast<int>(i);
    }
    return -1;
}

ProjectionArea* PatternGrid::GetArea(int index) {
    return IsValidIndex(index) ? &m_areas[index] : nullptr;
}

const ProjectionArea* PatternGrid::GetArea(int index) const {
    return IsValidIndex(index) ? &m_areas[index] : nullptr;
}
