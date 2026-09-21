#ifndef APPLICATION_H
#define APPLICATION_H

#include <chrono>
#include <string>

#include "anim/AnimationController.h"
#include "calib/CalibrationController.h"
#include "display/DisplayManager.h"
#include "model/PatternConfig.h"
#include "model/PatternGrid.h"
#include "render/RenderEngine.h"

struct AppOptions {
    int          monitorIndex = 1;  // 0-indexed; 1 = second monitor
    std::wstring configPath;        // Empty means "resolve automatically"
    std::string  animationPath;     // Path to .txt animation file
    bool         startInCalibration = false;
};

enum class AppMode {
    Show,        // Projection output: areas only, no overlay
    Calibration  // Interactive geometry editing with on-screen overlay
};

class Application {
public:
    Application();
    ~Application();

    bool Initialize(const AppOptions& options);
    void Run();
    void Quit();

    // Area control (by vector index, not by persisted id)
    void SetAreaVisible(int index, bool visible);
    void ToggleArea(int index);
    void SetAllAreasVisible(bool visible);

    AppMode GetMode() const { return m_mode; }
    void SetMode(AppMode mode);

    PatternGrid& GetGrid() { return m_grid; }
    const PatternGrid& GetGrid() const { return m_grid; }

    AnimationController& GetAnimation() { return m_animController; }
    const AnimationController& GetAnimation() const { return m_animController; }

private:
    void ProcessEvents();
    void HandleKeyDown(WPARAM key);
    void HandleShowModeKey(WPARAM key);
    void HandleLeftClick();
    void Update(double deltaTime);
    void Render();
    void RenderCalibrationOverlay();
    void PrintControls() const;
    void CycleAnimation();

    DisplayManager        m_displayManager;
    RenderEngine          m_renderEngine;
    PatternGrid           m_grid;
    CalibrationController m_calibration;
    AnimationController   m_animController;

    bool    m_isRunning;
    int     m_targetMonitorIndex;
    AppMode m_mode;

    std::wstring      m_configPath;
    PatternConfigData m_configData;
};

#endif // APPLICATION_H
