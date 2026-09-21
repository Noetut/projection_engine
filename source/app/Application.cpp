#include "Application.h"

#include <algorithm>
#include <iostream>
#include <sstream>
#include <thread>
#include <vector>
#include <mmsystem.h>
#include <timeapi.h>

#include "anim/AnimationParser.h"
#include "util/StringUtil.h"

namespace {

// --- Frame pacing ---------------------------------------------------------
const double kTargetFPS = 60.0;

// --- Calibration overlay palette ------------------------------------------
// Deliberately dim for the unselected areas: calibration happens with the
// projector live, pointed at a physical scene.
const COLORREF kDimOutline      = RGB(70, 70, 70);
const COLORREF kSelectedOutline = RGB(0, 180, 255);
const COLORREF kHandleFill      = RGB(255, 200, 0);
const COLORREF kHandleBorder    = RGB(0, 0, 0);
const COLORREF kActiveHandle    = RGB(255, 40, 40);
const COLORREF kCenterCross     = RGB(0, 180, 255);
const COLORREF kHudText         = RGB(220, 220, 220);
const COLORREF kLabelText       = RGB(140, 140, 140);

// --- Transparency levels (0 = invisible, 255 = solid) ---------------------
const BYTE kHudBgAlpha          = 120;  // Semi-transparent HUD box (~47% opacity, squares show through)
const BYTE kHudTextAlpha        = 180;  // Semi-transparent HUD text (~70% opacity)
const BYTE kLabelBgAlpha        = 100;  // Semi-transparent area label backing box (~39% opacity)
const BYTE kLabelTextAlpha      = 180;  // Semi-transparent area label text (~70% opacity)

// --- Calibration overlay layout (pixels) ----------------------------------
const int kHandleHalfSize        = 4;
const int kActiveHandleHalfSize  = 7;
const int kOutlineThickness      = 1;
const int kSelectedOutlineWidth  = 3;
const int kCenterCrossArm        = 12;
const int kAreaLabelInset        = 4;  // From the area's top-left corner
const int kHudOriginX            = 20;
const int kHudOriginY            = 20;

} // namespace

Application::Application()
    : m_isRunning(false)
    , m_targetMonitorIndex(1)
    , m_mode(AppMode::Show)
{
}

Application::~Application() {
}

// ---------------------------------------------------------------------------
// Start-up
// ---------------------------------------------------------------------------

bool Application::Initialize(const AppOptions& options) {
    m_targetMonitorIndex = options.monitorIndex;

    std::cout << "[Application] Initializing Patron Animation App..." << std::endl;

    // Step 1: Load the projection geometry. config/pattern_config.json is the
    // only source of truth; nothing is compiled into the binary.
    m_configPath = PatternConfig::ResolvePath(options.configPath);
    std::cout << "[Application] Config path: " << WideToUtf8(m_configPath) << std::endl;

    std::string configError;
    if (PatternConfig::Load(m_configPath, m_configData, configError)) {
        std::cout << "[Application] Loaded " << m_configData.areas.size()
                  << " projection area(s) from config." << std::endl;
    } else {
        std::cerr << "[Application] WARNING: could not load config (" << configError << ")."
                  << std::endl;
        std::cerr << "[Application] Starting with an EMPTY grid. Enter calibration mode "
                     "with [F1] and press [N] to create areas." << std::endl;
        m_configData = PatternConfigData();
    }
    m_grid.SetAreas(m_configData.areas);

    // Step 2: Create full-screen window on the target monitor
    HWND hwnd = m_displayManager.CreateWindowOnMonitor(m_targetMonitorIndex,
                                                       L"Patrón Animation - Grid Display");
    if (!hwnd) {
        std::cerr << "[Application] ERROR: DisplayManager failed to create window." << std::endl;
        return false;
    }

    // Step 3: Initialize double-buffered GDI rendering engine
    if (!m_renderEngine.Initialize(hwnd, m_displayManager.GetWidth(), m_displayManager.GetHeight())) {
        std::cerr << "[Application] ERROR: RenderEngine failed to initialize." << std::endl;
        return false;
    }

    // The stored coordinates are absolute pixels, so a monitor that does not
    // match the calibration canvas will show the pattern displaced.
    if (m_displayManager.GetWidth()  != m_configData.canvasWidth ||
        m_displayManager.GetHeight() != m_configData.canvasHeight) {
        std::cerr << "[Application] WARNING: monitor is "
                  << m_displayManager.GetWidth() << "x" << m_displayManager.GetHeight()
                  << " but the config canvas is "
                  << m_configData.canvasWidth << "x" << m_configData.canvasHeight
                  << ". Areas will not line up; recalibrate for this display." << std::endl;
    }

    // Step 4: Wire up calibration
    m_calibration.Attach(&m_grid, m_configPath,
                         m_configData.canvasWidth, m_configData.canvasHeight,
                         m_configData.referenceImage);
    m_calibration.SetSavedSnapshot(m_configData.areas);

    // Initial visibility comes from the config rather than being forced on:
    // the "visible" flag per area is persisted, so it is what the config says.
    std::cout << "[Application] Initialization complete! Target: Monitor #"
              << m_targetMonitorIndex << std::endl;

    // Step 5: Wire up animation sequence
    std::string resolvedAnimPath = AnimationParser::ResolvePath(options.animationPath);
    std::string animError;
    if (m_animController.LoadFromFile(resolvedAnimPath, animError)) {
        std::cout << "[Application] Animation loaded: " << resolvedAnimPath << std::endl;
        m_animController.PreloadMedia(m_renderEngine);
    } else if (!options.animationPath.empty()) {
        std::cerr << "[Application] WARNING: Failed to load animation ("
                  << options.animationPath << "): " << animError << std::endl;
    }

    PrintControls();

    if (options.startInCalibration) {
        SetMode(AppMode::Calibration);
    }

    m_isRunning = true;
    return true;
}

void Application::PrintControls() const {
    std::cout << "---------------------------------------------------------" << std::endl;
    std::cout << "                    SHOW MODE                            " << std::endl;
    std::cout << "  [Space] Play / Pause animation                         " << std::endl;
    std::cout << "  [Click / Enter] Trigger next animation segment         " << std::endl;
    std::cout << "  [R]     Restart animation                              " << std::endl;
    std::cout << "  [Tab]   Switch to next animation script                " << std::endl;
    std::cout << "  [0 - 9] Toggle individual area ON/OFF (by ID)          " << std::endl;
    std::cout << "  [A]     Turn ALL areas ON                              " << std::endl;
    std::cout << "  [O]     Turn ALL areas OFF (clear to black)            " << std::endl;
    std::cout << "  [F1]    Enter CALIBRATION mode                         " << std::endl;
    std::cout << "  [ESC]   Exit application                               " << std::endl;
    std::cout << "---------------------------------------------------------" << std::endl;
}

// ---------------------------------------------------------------------------
// Area visibility and mode switching
// ---------------------------------------------------------------------------

void Application::SetAreaVisible(int index, bool visible) {
    m_grid.SetAreaVisible(index, visible);
}

void Application::ToggleArea(int index) {
    m_grid.ToggleArea(index);
}

void Application::SetAllAreasVisible(bool visible) {
    m_grid.SetAllVisible(visible);
}

void Application::SetMode(AppMode mode) {
    if (m_mode == mode) return;

    if (m_mode == AppMode::Calibration) {
        m_calibration.OnExit();
        m_renderEngine.ClearQuadCache();
    }

    m_mode = mode;

    // Visibility is deliberately left alone in both directions. The overlay
    // draws every area regardless of its "visible" flag, so there is no need to
    // light them up for editing, and not touching the flags means a save from
    // calibration cannot silently turn hidden areas back on.
    if (m_mode == AppMode::Calibration) {
        m_calibration.OnEnter();
        m_animController.Pause();
    } else {
        std::cout << "[Application] Back in SHOW mode." << std::endl;
        if (m_animController.HasSequence()) {
            m_animController.Play();
        }
    }
}

// ---------------------------------------------------------------------------
// Input
//
// Calibration keys are handled by CalibrationController; only show-mode keys
// and the mode/quit keys live here.
// ---------------------------------------------------------------------------

void Application::HandleShowModeKey(WPARAM key) {
    if (key == VK_SPACE) {
        m_animController.TogglePlayPause();
        return;
    }

    if (key == VK_RETURN) {
        m_animController.TriggerClick(m_grid);
        return;
    }

    if (key == 'R') {
        m_animController.Restart(m_grid);
        return;
    }

    if (key == VK_TAB) {
        CycleAnimation();
        return;
    }

    if (key >= '0' && key <= '9') {
        int id = static_cast<int>(key - '0');
        int index = m_grid.FindIndexById(id);
        if (index < 0 && id > 0) {
            index = id - 1; // fallback
        }
        if (index < 0 || index >= static_cast<int>(m_grid.GetCount())) {
            std::cout << "[Application] No area with ID or index " << id << "." << std::endl;
            return;
        }
        m_grid.ToggleArea(index);
        const auto* area = m_grid.GetArea(index);
        if (area) {
            std::cout << "[Application] Area ID " << area->id << " [" << area->name << "] toggled -> "
                      << (area->isVisible ? "ON" : "OFF") << std::endl;
        }
        return;
    }

    if (key == 'A') {
        SetAllAreasVisible(true);
        std::cout << "[Application] All areas turned ON." << std::endl;
        return;
    }

    if (key == 'O') {
        SetAllAreasVisible(false);
        std::cout << "[Application] All areas turned OFF (Clear)." << std::endl;
        return;
    }
}

void Application::HandleKeyDown(WPARAM key) {
    if (key == VK_ESCAPE) {
        m_isRunning = false;
        return;
    }

    if (key == VK_F1) {
        SetMode(m_mode == AppMode::Show ? AppMode::Calibration : AppMode::Show);
        return;
    }

    const bool shift = (GetKeyState(VK_SHIFT)   & 0x8000) != 0;
    const bool ctrl  = (GetKeyState(VK_CONTROL) & 0x8000) != 0;

    if (m_mode == AppMode::Calibration) {
        m_calibration.HandleKey(key, shift, ctrl);
        return;
    }

    HandleShowModeKey(key);
}

void Application::HandleLeftClick() {
    if (m_mode == AppMode::Show) {
        m_animController.TriggerClick(m_grid);
    }
}

void Application::ProcessEvents() {
    MSG msg;
    while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            m_isRunning = false;
        } else if (msg.message == WM_KEYDOWN && msg.hwnd == m_displayManager.GetHWND()) {
            // Filtered by window: the thread queue can carry messages that are
            // not meant for the projection window.
            HandleKeyDown(msg.wParam);
        } else if (msg.message == WM_LBUTTONDOWN && msg.hwnd == m_displayManager.GetHWND()) {
            HandleLeftClick();
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

// ---------------------------------------------------------------------------
// Frame loop: Update -> Render -> Run
// ---------------------------------------------------------------------------

void Application::Update(double deltaTime) {
    if (m_mode == AppMode::Calibration) {
        m_calibration.Update(deltaTime);
        return;
    }

    // Show mode: advance animation sequence & background video playback
    m_animController.Update(deltaTime, m_grid);
}

void Application::RenderCalibrationOverlay() {
    m_renderEngine.RenderBlack();

    const auto& areas = m_grid.GetAreas();
    const int selected = m_calibration.SelectedIndex();
    const int corner = m_calibration.SelectedCorner();

    for (size_t i = 0; i < areas.size(); ++i) {
        const auto& area = areas[i];
        const bool isSelected = (static_cast<int>(i) == selected);

        if (isSelected) {
            m_renderEngine.FillQuadHalftone(area.quad, area.color);
        }
        m_renderEngine.DrawQuadOutline(area.quad,
                                       isSelected ? kSelectedOutline : kDimOutline,
                                       isSelected ? kSelectedOutlineWidth : kOutlineThickness);

        // ID label, so the operator knows which digit selects this area.
        std::wostringstream label;
        label << L"[" << area.id << L"] " << Utf8ToWide(area.name);
        if (area.type == "text") {
            label << L" (TEXT)";
        }
        m_renderEngine.DrawHudText(area.quad.corners[0].x + kAreaLabelInset,
                                   area.quad.corners[0].y + kAreaLabelInset,
                                   label.str(),
                                   isSelected ? kSelectedOutline : kLabelText,
                                   kLabelBgAlpha, kLabelTextAlpha);

        if (area.type == "text" && isSelected) {
            std::string preview = area.text.empty() ? "Aa Texto" : area.text;
            m_renderEngine.DrawQuadText(area.quad, preview, area.fontFace, area.fontSize, RGB(0, 220, 255));
        }

        if (!isSelected) continue;

        Point2i center = { static_cast<int>(area.quad.CenterX()),
                           static_cast<int>(area.quad.CenterY()) };
        m_renderEngine.DrawCross(center, kCenterCrossArm, kCenterCross);

        for (int c = 0; c < 4; ++c) {
            const bool isActiveCorner = (c == corner);
            if (isActiveCorner) {
                // Blinking makes the active corner unmistakable from across
                // the room, where colour alone is easy to miss.
                if (m_calibration.HandleBlinkOn()) {
                    m_renderEngine.DrawHandle(area.quad.corners[c], kActiveHandleHalfSize,
                                              kActiveHandle, kHandleBorder);
                }
            } else {
                m_renderEngine.DrawHandle(area.quad.corners[c], kHandleHalfSize,
                                          kHandleFill, kHandleBorder);
            }
        }
    }

    if (m_calibration.ShowHud()) {
        m_renderEngine.DrawHudText(kHudOriginX, kHudOriginY,
                                   m_calibration.BuildHudText(), kHudText,
                                   kHudBgAlpha, kHudTextAlpha);
    }
}

void Application::Render() {
    m_renderEngine.BeginFrame();

    if (m_mode == AppMode::Calibration) {
        RenderCalibrationOverlay();
    } else {
        int bgW = 0, bgH = 0;
        const BYTE* bgPixels = m_animController.GetBackgroundVideoFrame(bgW, bgH);
        float bgBrightness = m_animController.GetBackgroundVideoBrightness();
        m_renderEngine.RenderAreas(m_grid.GetAreas(), bgPixels, bgW, bgH, bgBrightness);
    }

    m_renderEngine.EndFrame();
}

void Application::Run() {
    timeBeginPeriod(1);

    using clock = std::chrono::high_resolution_clock;
    auto previousTime = clock::now();

    double targetFPS = static_cast<double>(m_displayManager.GetRefreshRate());
    if (targetFPS < 30.0 || targetFPS > 360.0) targetFPS = 60.0;
    const std::chrono::duration<double> targetFrameDuration(1.0 / targetFPS);

    std::cout << "[Application] Running main loop locked to " << targetFPS << " FPS" << std::endl;

    while (m_isRunning) {
        auto currentTime = clock::now();
        std::chrono::duration<double> elapsedTime = currentTime - previousTime;
        previousTime = currentTime;

        double deltaTime = elapsedTime.count();
        if (deltaTime > 0.1) deltaTime = 0.1;
        if (deltaTime < 0.0) deltaTime = 0.0;

        ProcessEvents();
        Update(deltaTime);
        Render();

        // Cap frame rate precisely at 60 FPS without Windows timer jitter
        auto frameEndTime = clock::now();
        auto frameDuration = frameEndTime - currentTime;
        if (frameDuration < targetFrameDuration) {
            auto sleepDuration = targetFrameDuration - frameDuration;
            if (sleepDuration > std::chrono::milliseconds(2)) {
                std::this_thread::sleep_for(sleepDuration - std::chrono::milliseconds(1));
            }
            while (clock::now() - currentTime < targetFrameDuration) {
                // Precise spin-wait for sub-millisecond accuracy
            }
        }
    }

    timeEndPeriod(1);

    if (m_mode == AppMode::Calibration && m_calibration.IsDirty()) {
        std::cerr << "[Application] WARNING: exited with UNSAVED calibration changes." << std::endl;
    }

    std::cout << "[Application] Exiting cleanly." << std::endl;
}

void Application::Quit() {
    m_isRunning = false;
}

void Application::CycleAnimation() {
    std::vector<std::string> animFiles;
    const char* searchDirs[] = { "animations", "../animations" };

    for (const char* dir : searchDirs) {
        std::wstring searchPattern = AnsiToWide(std::string(dir) + "/*.txt");
        WIN32_FIND_DATAW findData;
        HANDLE hFind = FindFirstFileW(searchPattern.c_str(), &findData);
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                    std::string fileName = WideToUtf8(findData.cFileName);
                    std::string fullPath = std::string(dir) + "/" + fileName;

                    bool exists = false;
                    for (const auto& existing : animFiles) {
                        if (existing.find(fileName) != std::string::npos) {
                            exists = true;
                            break;
                        }
                    }
                    if (!exists) {
                        animFiles.push_back(fullPath);
                    }
                }
            } while (FindNextFileW(hFind, &findData));
            FindClose(hFind);
        }
    }

    if (animFiles.empty()) {
        std::cout << "[Application] No animation files found in animations/ directory." << std::endl;
        return;
    }

    // Sort for deterministic cycling order
    std::sort(animFiles.begin(), animFiles.end());

    size_t currentIndex = 0;
    const std::string& currentPath = m_animController.FilePath();
    for (size_t i = 0; i < animFiles.size(); ++i) {
        if (!currentPath.empty() && (animFiles[i] == currentPath ||
            currentPath.find(animFiles[i]) != std::string::npos ||
            animFiles[i].find(currentPath) != std::string::npos)) {
            currentIndex = i;
            break;
        }
    }

    size_t nextIndex = (currentIndex + 1) % animFiles.size();
    std::string nextPath = animFiles[nextIndex];

    std::string err;
    if (m_animController.LoadFromFile(nextPath, err)) {
        m_animController.PreloadMedia(m_renderEngine);
        m_animController.Restart(m_grid);
        std::cout << "[Application] Switched to animation: " << nextPath
                  << " ('" << m_animController.SequenceName() << "')" << std::endl;
    } else {
        std::cerr << "[Application] Failed to load " << nextPath << ": " << err << std::endl;
    }
}
