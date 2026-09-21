#include "CalibrationController.h"

#include <iostream>
#include <sstream>

#include "util/StringUtil.h"

namespace {

// Only used until Attach() supplies the real canvas from the config.
const int kFallbackCanvasWidth  = 1920;
const int kFallbackCanvasHeight = 1080;

// Index order mirrors the Corner enum in model/Geometry.h.
const char* const kCornerNames[4] = {
    "TOP-LEFT", "TOP-RIGHT", "BOTTOM-RIGHT", "BOTTOM-LEFT"
};

// Sentinel meaning "no nudge run is open"; never equal to a real corner index
// nor to WHOLE_AREA, so the first nudge after any other key always opens an
// undo entry.
const int kNoNudgeTarget = -2;

} // namespace

// Member order here must match the declaration order in the header, otherwise
// -Wreorder fires.
CalibrationController::CalibrationController()
    : m_grid(nullptr)
    , m_canvasWidth(kFallbackCanvasWidth)
    , m_canvasHeight(kFallbackCanvasHeight)
    , m_selectedIndex(0)
    , m_selectedCorner(WHOLE_AREA)
    , m_showHud(true)
    , m_handleBlinkTimer(0.0)
    , m_handleBlinkOn(true)
    , m_lastStepSize(STEP_FINE)
    , m_now(0.0)
    , m_lastNudgeTime(-1.0e9)
    , m_lastNudgeIndex(-1)
    , m_lastNudgeCorner(kNoNudgeTarget)
    , m_dirty(false)
{
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void CalibrationController::Attach(PatternGrid* grid, const std::wstring& configPath,
                                   int canvasWidth, int canvasHeight,
                                   const std::string& referenceImage) {
    m_grid = grid;
    m_configPath = configPath;
    m_canvasWidth = canvasWidth;
    m_canvasHeight = canvasHeight;
    m_referenceImage = referenceImage;
    EnsureSelectionValid();
}

void CalibrationController::SetSavedSnapshot(const std::vector<ProjectionArea>& areas) {
    m_savedAreas = areas;
    m_dirty = false;
}

void CalibrationController::OnEnter() {
    EnsureSelectionValid();
    BreakNudgeRun();
    m_undoStack.clear();
    m_statusMessage = "Calibration mode active.";
    std::cout << "[Calibration] ENTERED calibration mode." << std::endl;
}

void CalibrationController::OnExit() {
    if (m_dirty) {
        std::cout << "[Calibration] WARNING: leaving with UNSAVED changes. "
                     "Re-enter with [F1] and press [Ctrl+S] to save." << std::endl;
    }
    std::cout << "[Calibration] EXITED calibration mode." << std::endl;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

int CalibrationController::StepSize(bool shift, bool ctrl) const {
    if (shift && ctrl) return STEP_JUMP;
    if (shift)         return STEP_COARSE;
    return STEP_FINE;
}

const char* CalibrationController::CornerName(int corner) const {
    if (corner < 0 || corner > 3) return "WHOLE AREA";
    return kCornerNames[corner];
}

// ---------------------------------------------------------------------------
// Undo and dirty tracking
// ---------------------------------------------------------------------------

void CalibrationController::PushUndo() {
    if (!m_grid) return;

    m_undoStack.push_back(m_grid->GetAreas());
    if (m_undoStack.size() > MAX_UNDO_DEPTH) {
        m_undoStack.erase(m_undoStack.begin());
    }
}

// Derived from an exact comparison against the on-disk snapshot rather than a
// sticky flag, so that undoing back to the saved state clears the warning.
void CalibrationController::RecomputeDirty() {
    m_dirty = m_grid ? !SameAsPersisted(m_grid->GetAreas(), m_savedAreas) : false;
}

void CalibrationController::Undo() {
    if (!m_grid || m_undoStack.empty()) {
        m_statusMessage = "Nothing to undo.";
        return;
    }

    m_grid->SetAreas(m_undoStack.back());
    m_undoStack.pop_back();
    EnsureSelectionValid();
    RecomputeDirty();
    m_statusMessage = "Undo applied (" + std::to_string(m_undoStack.size()) + " level(s) left).";
}

void CalibrationController::BreakNudgeRun() {
    m_lastNudgeIndex = -1;
    m_lastNudgeCorner = kNoNudgeTarget;
}

// ---------------------------------------------------------------------------
// Selection
// ---------------------------------------------------------------------------

void CalibrationController::EnsureSelectionValid() {
    if (!m_grid || m_grid->GetCount() == 0) {
        m_selectedIndex = -1;
        return;
    }

    const int count = static_cast<int>(m_grid->GetCount());
    if (m_selectedIndex < 0)      m_selectedIndex = 0;
    if (m_selectedIndex >= count) m_selectedIndex = count - 1;
}

void CalibrationController::SelectRelative(int delta) {
    if (!m_grid || m_grid->GetCount() == 0) return;

    const int count = static_cast<int>(m_grid->GetCount());
    m_selectedIndex = ((m_selectedIndex + delta) % count + count) % count;
    m_statusMessage.clear();
}

void CalibrationController::CycleCorner(int delta) {
    // Five states: TL -> TR -> BR -> BL -> WHOLE -> TL
    const int kWholeState = 4;
    const int kStateCount = 5;
    int state = (m_selectedCorner == WHOLE_AREA) ? kWholeState : m_selectedCorner;
    state = ((state + delta) % kStateCount + kStateCount) % kStateCount;
    m_selectedCorner = (state == kWholeState) ? WHOLE_AREA : state;
    m_statusMessage.clear();
}

// ---------------------------------------------------------------------------
// Editing
// ---------------------------------------------------------------------------

void CalibrationController::Nudge(int dx, int dy) {
    if (!m_grid) return;

    ProjectionArea* area = m_grid->GetArea(m_selectedIndex);
    if (!area) return;

    // A held arrow key auto-repeats; only the first step of such a run gets its
    // own undo entry, so one Ctrl+Z reverts the whole drag instead of 1 px.
    const bool continuesRun = (m_selectedIndex  == m_lastNudgeIndex) &&
                              (m_selectedCorner == m_lastNudgeCorner) &&
                              ((m_now - m_lastNudgeTime) < NUDGE_COALESCE_SECONDS);
    if (!continuesRun) {
        PushUndo();
    }
    m_lastNudgeIndex  = m_selectedIndex;
    m_lastNudgeCorner = m_selectedCorner;
    m_lastNudgeTime   = m_now;

    if (m_selectedCorner == WHOLE_AREA) {
        area->quad.Translate(dx, dy);
    } else {
        area->quad.MoveCorner(m_selectedCorner, dx, dy);
    }
    RecomputeDirty();
    m_statusMessage.clear();
}

void CalibrationController::ResetSelectedArea() {
    if (!m_grid) return;

    ProjectionArea* area = m_grid->GetArea(m_selectedIndex);
    if (!area) return;

    for (const auto& saved : m_savedAreas) {
        if (saved.id == area->id) {
            PushUndo();
            *area = saved;
            RecomputeDirty();
            m_statusMessage = "Area restored to last saved state.";
            return;
        }
    }
    m_statusMessage = "This area has no saved state to restore.";
}

void CalibrationController::CreateArea() {
    if (!m_grid) return;

    PushUndo();
    m_selectedIndex = m_grid->CreateArea(m_canvasWidth, m_canvasHeight);
    m_selectedCorner = WHOLE_AREA;
    RecomputeDirty();

    const ProjectionArea* area = m_grid->GetArea(m_selectedIndex);
    m_statusMessage = area ? ("Created " + area->name) : "Created area.";
    std::cout << "[Calibration] " << m_statusMessage << std::endl;
}

void CalibrationController::CreateTextArea() {
    if (!m_grid) return;

    PushUndo();
    m_selectedIndex = m_grid->CreateTextArea(m_canvasWidth, m_canvasHeight);
    m_selectedCorner = WHOLE_AREA;
    RecomputeDirty();

    const ProjectionArea* area = m_grid->GetArea(m_selectedIndex);
    m_statusMessage = area ? ("Created text box " + area->name) : "Created text box.";
    std::cout << "[Calibration] " << m_statusMessage << std::endl;
}

void CalibrationController::ToggleAreaType() {
    if (!m_grid) return;
    ProjectionArea* area = m_grid->GetArea(m_selectedIndex);
    if (!area) return;

    PushUndo();
    if (area->type == "text") {
        area->type = "quad";
        m_statusMessage = "Converted " + area->name + " to QUAD";
    } else {
        area->type = "text";
        m_statusMessage = "Converted " + area->name + " to TEXT BOX";
    }
    RecomputeDirty();
    std::cout << "[Calibration] " << m_statusMessage << std::endl;
}

void CalibrationController::DuplicateArea() {
    if (!m_grid) return;

    const int source = m_selectedIndex;
    if (!m_grid->GetArea(source)) return;

    PushUndo();
    const int created = m_grid->DuplicateArea(source);
    if (created < 0) {
        m_undoStack.pop_back();
        return;
    }

    m_selectedIndex = created;
    m_selectedCorner = WHOLE_AREA;
    RecomputeDirty();

    const ProjectionArea* area = m_grid->GetArea(m_selectedIndex);
    m_statusMessage = area ? ("Duplicated as " + area->name) : "Duplicated area.";
    std::cout << "[Calibration] " << m_statusMessage << std::endl;
}

void CalibrationController::DeleteSelectedArea() {
    if (!m_grid) return;

    const ProjectionArea* area = m_grid->GetArea(m_selectedIndex);
    if (!area) return;

    const std::string name = area->name;
    PushUndo();
    m_grid->RemoveArea(m_selectedIndex);
    EnsureSelectionValid();
    RecomputeDirty();
    m_statusMessage = "Deleted " + name + " (Ctrl+Z to undo).";
    std::cout << "[Calibration] " << m_statusMessage << std::endl;
}

// ---------------------------------------------------------------------------
// Persistence
// ---------------------------------------------------------------------------

bool CalibrationController::Save() {
    if (!m_grid) return false;

    PatternConfigData data;
    data.canvasWidth = m_canvasWidth;
    data.canvasHeight = m_canvasHeight;
    data.referenceImage = m_referenceImage;
    data.areas = m_grid->GetAreas();

    std::string error;
    if (!PatternConfig::Save(m_configPath, data, error)) {
        m_statusMessage = "SAVE FAILED: " + error;
        std::cerr << "[Calibration] " << m_statusMessage << std::endl;
        return false;
    }

    m_savedAreas = data.areas;
    m_dirty = false;
    m_statusMessage = "Saved " + std::to_string(data.areas.size()) + " area(s) to config.";
    std::cout << "[Calibration] " << m_statusMessage << std::endl;
    return true;
}

bool CalibrationController::Reload() {
    if (!m_grid) return false;

    PatternConfigData data;
    std::string error;
    if (!PatternConfig::Load(m_configPath, data, error)) {
        m_statusMessage = "RELOAD FAILED: " + error;
        std::cerr << "[Calibration] " << m_statusMessage << std::endl;
        return false;
    }

    m_grid->SetAreas(data.areas);
    m_canvasWidth = data.canvasWidth;
    m_canvasHeight = data.canvasHeight;
    m_referenceImage = data.referenceImage;
    m_savedAreas = data.areas;
    m_undoStack.clear();
    m_dirty = false;
    EnsureSelectionValid();

    m_statusMessage = "Reloaded " + std::to_string(data.areas.size()) + " area(s) from disk.";
    std::cout << "[Calibration] " << m_statusMessage << std::endl;
    return true;
}

// ---------------------------------------------------------------------------
// Key handling
//
// The full key map is documented in README.md under "Calibration mode".
// ---------------------------------------------------------------------------

bool CalibrationController::HandleKey(WPARAM key, bool shift, bool ctrl) {
    const int step = StepSize(shift, ctrl);

    // Anything other than an arrow key ends a coalesced nudge run, so that
    // changing selection or running a command always starts a fresh undo entry.
    const bool isArrow = (key == VK_LEFT || key == VK_RIGHT ||
                          key == VK_UP   || key == VK_DOWN);
    if (!isArrow) {
        BreakNudgeRun();
    }

    switch (key) {
    // --- Area selection ---------------------------------------------------
    case VK_TAB:
        SelectRelative(shift ? -1 : 1);
        return true;

    // --- Corner selection: numpad layout mirrors the corner positions.
    // Both the numpad and the navigation-cluster virtual keys are accepted
    // because with NumLock off Windows reports VK_HOME/VK_END/etc. instead.
    case VK_NUMPAD7: case VK_HOME:  m_selectedCorner = 0;          return true;
    case VK_NUMPAD9: case VK_PRIOR: m_selectedCorner = 1;          return true;
    case VK_NUMPAD3: case VK_NEXT:  m_selectedCorner = 2;          return true;
    case VK_NUMPAD1: case VK_END:   m_selectedCorner = 3;          return true;
    case VK_NUMPAD5: case VK_CLEAR: m_selectedCorner = WHOLE_AREA; return true;

    case 'Q': CycleCorner(-1); return true;
    case 'E': CycleCorner(+1); return true;

    // --- Movement ---------------------------------------------------------
    case VK_LEFT:  m_lastStepSize = step; Nudge(-step, 0); return true;
    case VK_RIGHT: m_lastStepSize = step; Nudge(+step, 0); return true;
    case VK_UP:    m_lastStepSize = step; Nudge(0, -step); return true;
    case VK_DOWN:  m_lastStepSize = step; Nudge(0, +step); return true;

    // --- Lifecycle --------------------------------------------------------
    case 'N': CreateArea(); return true;
    case 'T':
        if (shift) {
            ToggleAreaType();
        } else {
            CreateTextArea();
        }
        return true;
    case 'D': DuplicateArea(); return true;

    case VK_DELETE:
        // Shift is required: a hand-calibrated area is expensive to recreate.
        if (shift) {
            DeleteSelectedArea();
        } else {
            m_statusMessage = "Press Shift+Delete to confirm deletion.";
        }
        return true;

    case 'Z':
        if (ctrl) { Undo(); return true; }
        return false;

    case 'R': ResetSelectedArea(); return true;

    // --- Persistence ------------------------------------------------------
    case 'S':
        if (ctrl) { Save(); return true; }
        return false;

    case 'L': Reload(); return true;

    case 'H':
        m_showHud = !m_showHud;
        return true;

    default:
        break;
    }

    // Digits 0-9 select an area directly by ID or index.
    if (key >= '0' && key <= '9') {
        const int id = static_cast<int>(key - '0');
        int index = m_grid ? m_grid->FindIndexById(id) : -1;
        if (index < 0 && id > 0) {
            index = id - 1; // legacy fallback
        }
        if (m_grid && index >= 0 && index < static_cast<int>(m_grid->GetCount())) {
            m_selectedIndex = index;
            m_statusMessage.clear();
        } else {
            m_statusMessage = "No area with ID " + std::to_string(id) + ".";
        }
        return true;
    }

    return false;
}

// ---------------------------------------------------------------------------
// Frame update and HUD
// ---------------------------------------------------------------------------

void CalibrationController::Update(double deltaTime) {
    m_now += deltaTime;

    m_handleBlinkTimer += deltaTime;
    if (m_handleBlinkTimer >= HANDLE_BLINK_SECONDS) {
        m_handleBlinkTimer -= HANDLE_BLINK_SECONDS;
        m_handleBlinkOn = !m_handleBlinkOn;
    }
}

std::wstring CalibrationController::BuildHudText() const {
    std::ostringstream out;

    const int count = m_grid ? static_cast<int>(m_grid->GetCount()) : 0;
    out << "CALIBRATION MODE   [F1] back to show   [H] hide panel\n";

    const ProjectionArea* area = m_grid ? m_grid->GetArea(m_selectedIndex) : nullptr;
    if (!area) {
        out << "No areas defined. Press [N] to create one.\n";
    } else {
        RECT bbox = area->quad.BoundingBox();
        out << "Area " << (m_selectedIndex + 1) << "/" << count
            << "   id " << area->id << "   " << area->name
            << (area->type == "text" ? "   [TEXT BOX]" : "") << "\n"
            << "Target: " << CornerName(m_selectedCorner);

        if (m_selectedCorner != WHOLE_AREA) {
            out << "   (" << area->quad.corners[m_selectedCorner].x
                << ", "  << area->quad.corners[m_selectedCorner].y << ")";
        }
        out << "\n";

        out << "Corners  TL(" << area->quad.corners[0].x << "," << area->quad.corners[0].y << ")"
            << "  TR(" << area->quad.corners[1].x << "," << area->quad.corners[1].y << ")"
            << "  BR(" << area->quad.corners[2].x << "," << area->quad.corners[2].y << ")"
            << "  BL(" << area->quad.corners[3].x << "," << area->quad.corners[3].y << ")\n";

        out << "BBox " << (bbox.right - bbox.left) << "x" << (bbox.bottom - bbox.top)
            << " at (" << bbox.left << "," << bbox.top << ")"
            << (area->quad.IsAxisAlignedRect() ? "   [axis-aligned]" : "   [keystoned]") << "\n";

        // The overlay draws hidden areas the same as visible ones, so the only
        // way to see the persisted flag during calibration is to report it.
        out << "Shown in show mode: " << (area->isVisible ? "yes" : "no") << "\n";
    }

    out << "Step " << m_lastStepSize << " px    Shift=10    Ctrl+Shift=50\n";
    out << "Unsaved changes: " << (m_dirty ? "YES" : "no")
        << "    Undo levels: " << m_undoStack.size() << "\n";

    out << "\n"
        << "Tab / Shift+Tab  previous / next area\n"
        << "0-9              select area by ID\n"
        << "Num 7 9 3 1      corner TL TR BR BL      Num 5  whole area\n"
        << "Q / E            cycle corner\n"
        << "Arrows           move target\n"
        << "N new quad   T new text box   Shift+T toggle type   D duplicate   Shift+Del delete\n"
        << "Ctrl+Z undo   R reset area   Ctrl+S save   L reload\n";

    if (!m_statusMessage.empty()) {
        out << "\n> " << m_statusMessage << "\n";
    }

    return Utf8ToWide(out.str());
}
