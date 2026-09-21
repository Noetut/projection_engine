#include "DisplayManager.h"

#include <iostream>

DisplayManager::DisplayManager()
    : m_hwnd(NULL)
    , m_hInstance(GetModuleHandle(NULL))
    , m_width(1920)
    , m_height(1080)
    , m_refreshRate(60)
{
}

DisplayManager::~DisplayManager() {
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = NULL;
    }
}

// The hdcMonitor and lprcMonitor parameters are required by the
// EnumDisplayMonitors callback signature but unused here, so they are left
// unnamed; GetMonitorInfoW supplies the rectangle instead.
BOOL CALLBACK DisplayManager::MonitorEnumProc(HMONITOR hMonitor, HDC, LPRECT, LPARAM dwData) {
    auto* displays = reinterpret_cast<std::vector<DisplayInfo>*>(dwData);
    MONITORINFOEXW mi;
    mi.cbSize = sizeof(MONITORINFOEXW);

    if (GetMonitorInfoW(hMonitor, &mi)) {
        DisplayInfo info;
        info.index = static_cast<int>(displays->size());
        info.hMonitor = hMonitor;
        info.rect = mi.rcMonitor;
        info.width = mi.rcMonitor.right - mi.rcMonitor.left;
        info.height = mi.rcMonitor.bottom - mi.rcMonitor.top;
        info.isPrimary = (mi.dwFlags & MONITORINFOF_PRIMARY) != 0;
        info.refreshRate = 60;

        DEVMODEW dm = {};
        dm.dmSize = sizeof(DEVMODEW);
        if (EnumDisplaySettingsW(mi.szDevice, ENUM_CURRENT_SETTINGS, &dm) && dm.dmDisplayFrequency > 0) {
            info.refreshRate = static_cast<int>(dm.dmDisplayFrequency);
        }

        displays->push_back(info);
    }
    return TRUE;
}

bool DisplayManager::EnumerateDisplays() {
    m_displays.clear();
    EnumDisplayMonitors(NULL, NULL, MonitorEnumProc, reinterpret_cast<LPARAM>(&m_displays));
    
    std::cout << "[DisplayManager] Found " << m_displays.size() << " connected monitor(s):" << std::endl;
    for (size_t i = 0; i < m_displays.size(); ++i) {
        const auto& d = m_displays[i];
        std::cout << "  Monitor #" << d.index 
                  << " (" << d.width << "x" << d.height << " @ " << d.refreshRate << " Hz)"
                  << " Pos: (" << d.rect.left << ", " << d.rect.top << ")"
                  << (d.isPrimary ? " [PRIMARY]" : "") << std::endl;
    }
    return !m_displays.empty();
}

LRESULT CALLBACK DisplayManager::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CLOSE:
        PostQuitMessage(0);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    // Key handling lives in Application::HandleKeyDown, which sniffs WM_KEYDOWN
    // off the queue before dispatch; handling ESC here too would duplicate it.
    case WM_ERASEBKGND:
        return 1; // Prevent GDI flicker
    }
    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

HWND DisplayManager::CreateWindowOnMonitor(int targetMonitorIndex, const wchar_t* windowTitle) {
    EnumerateDisplays();

    int selectedIndex = targetMonitorIndex;
    if (selectedIndex < 0 || selectedIndex >= static_cast<int>(m_displays.size())) {
        std::cout << "[DisplayManager] WARNING: Requested Monitor #" << targetMonitorIndex 
                  << " not found. Falling back to Monitor #0." << std::endl;
        selectedIndex = 0;
    }

    DisplayInfo targetDisplay;
    if (!m_displays.empty()) {
        targetDisplay = m_displays[selectedIndex];
    } else {
        // Fallback default if monitor enumeration returns empty
        targetDisplay.rect = { 0, 0, 1920, 1080 };
        targetDisplay.width = 1920;
        targetDisplay.height = 1080;
        targetDisplay.refreshRate = 60;
    }

    m_width = targetDisplay.width;
    m_height = targetDisplay.height;
    m_refreshRate = targetDisplay.refreshRate > 0 ? targetDisplay.refreshRate : 60;

    std::cout << "[DisplayManager] Creating full black window on Monitor #" << selectedIndex 
              << " (" << targetDisplay.width << "x" << targetDisplay.height << " @ " << m_refreshRate << " Hz)"
              << " at (" << targetDisplay.rect.left << ", " << targetDisplay.rect.top << ")" << std::endl;

    const wchar_t CLASS_NAME[] = L"PatronAnimationWindowClass";

    WNDCLASSW wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = m_hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    RegisterClassW(&wc);

    // Create a frameless borderless popup window covering the target monitor
    m_hwnd = CreateWindowExW(
        WS_EX_APPWINDOW | WS_EX_TOPMOST,
        CLASS_NAME,
        windowTitle,
        WS_POPUP | WS_VISIBLE,
        targetDisplay.rect.left,
        targetDisplay.rect.top,
        targetDisplay.width,
        targetDisplay.height,
        NULL,
        NULL,
        m_hInstance,
        NULL
    );

    if (!m_hwnd) {
        std::cerr << "[DisplayManager] ERROR: Failed to create window." << std::endl;
        return NULL;
    }

    ShowWindow(m_hwnd, SW_SHOW);
    UpdateWindow(m_hwnd);

    // Calibration is keyboard-driven, and this popup competes with the console
    // window for focus, so claim it explicitly.
    SetForegroundWindow(m_hwnd);
    SetFocus(m_hwnd);

    return m_hwnd;
}
