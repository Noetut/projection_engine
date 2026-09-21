#ifndef CALIBRATION_CONTROLLER_H
#define CALIBRATION_CONTROLLER_H

#include <windows.h>

#include <string>
#include <vector>

#include "model/PatternConfig.h"
#include "model/PatternGrid.h"

// Interactive calibration: selects a projection area, nudges either a single
// corner or the whole quad, creates/duplicates/deletes areas, and persists the
// result back to config/pattern_config.json.
//
// Owns no rendering: Application queries the selection state and draws the
// overlay through RenderEngine.
class CalibrationController {
public:
    // --- Tuning -----------------------------------------------------------
    // Everything adjustable about calibration behaviour lives here.

    // Target sentinel: -1 means "the whole area", all four corners at once.
    static constexpr int WHOLE_AREA = -1;

    // Nudge distances in pixels, picked by the Shift / Ctrl modifiers.
    static constexpr int STEP_FINE   = 1;   // no modifier
    static constexpr int STEP_COARSE = 10;  // Shift
    static constexpr int STEP_JUMP   = 50;  // Ctrl+Shift

    // Consecutive nudges of the same target inside this window share a single
    // undo entry, so a held arrow key cannot flood the stack.
    static constexpr double NUDGE_COALESCE_SECONDS = 0.5;
    static constexpr size_t MAX_UNDO_DEPTH = 50;

    // Blink period of the active corner handle. Blinking makes it unmistakable
    // at projector distance, where a colour difference alone is easy to miss.
    static constexpr double HANDLE_BLINK_SECONDS = 0.25;

    CalibrationController();

    void Attach(PatternGrid* grid, const std::wstring& configPath,
                int canvasWidth, int canvasHeight, const std::string& referenceImage);

    // Records the on-disk state so that R (reset area) and the dirty flag have
    // a baseline. Call after a successful load.
    void SetSavedSnapshot(const std::vector<ProjectionArea>& areas);

    void OnEnter();
    void OnExit();

    // Returns true when the key was consumed by calibration mode.
    bool HandleKey(WPARAM key, bool shift, bool ctrl);
    void Update(double deltaTime);

    // --- Queries used by the overlay renderer ------------------------------
    int  SelectedIndex() const { return m_selectedIndex; }
    int  SelectedCorner() const { return m_selectedCorner; }
    bool IsDirty() const { return m_dirty; }
    bool ShowHud() const { return m_showHud; }
    bool HandleBlinkOn() const { return m_handleBlinkOn; }
    std::wstring BuildHudText() const;

    // --- Persistence ------------------------------------------------------
    bool Save();
    bool Reload();

private:
    // --- Selection --------------------------------------------------------
    void EnsureSelectionValid();
    void SelectRelative(int delta);
    void CycleCorner(int delta);

    // --- Editing ----------------------------------------------------------
    void Nudge(int dx, int dy);
    void CreateArea();
    void CreateTextArea();
    void ToggleAreaType();
    void DuplicateArea();
    void DeleteSelectedArea();
    void ResetSelectedArea();

    // --- Undo and dirty tracking ------------------------------------------
    void PushUndo();
    void Undo();
    void RecomputeDirty();
    // Ends the current run of coalesced nudges, so the next one starts a fresh
    // undo entry.
    void BreakNudgeRun();

    // --- Helpers ----------------------------------------------------------
    int  StepSize(bool shift, bool ctrl) const;
    const char* CornerName(int corner) const;

    // --- Attached state ---------------------------------------------------
    PatternGrid* m_grid;
    std::wstring m_configPath;
    int          m_canvasWidth;
    int          m_canvasHeight;
    std::string  m_referenceImage;

    // --- Current selection ------------------------------------------------
    int m_selectedIndex;   // Vector index, or -1 when there are no areas
    int m_selectedCorner;  // 0..3, or WHOLE_AREA

    // --- View state -------------------------------------------------------
    bool   m_showHud;
    double m_handleBlinkTimer;
    bool   m_handleBlinkOn;
    int    m_lastStepSize;     // Reported by the HUD
    std::string m_statusMessage;

    // --- Nudge coalescing -------------------------------------------------
    // Held-down arrow keys auto-repeat about 30 times a second, so the last
    // nudge target and timestamp decide whether to open a new undo entry.
    double m_now;              // Accumulated from Update(deltaTime)
    double m_lastNudgeTime;
    int    m_lastNudgeIndex;
    int    m_lastNudgeCorner;

    // --- History ----------------------------------------------------------
    bool m_dirty;
    std::vector<ProjectionArea>              m_savedAreas;  // Last on-disk state
    std::vector<std::vector<ProjectionArea>> m_undoStack;
};

#endif // CALIBRATION_CONTROLLER_H
