#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <windows.h>

#include <string>
#include <vector>

struct DisplayInfo {
    int index;
    HMONITOR hMonitor;
    RECT rect; // Screen coordinates (left, top, right, bottom)
    int width;
    int height;
    int refreshRate; // Display refresh rate in Hz
    bool isPrimary;
};

class DisplayManager {
public:
    DisplayManager();
    ~DisplayManager();

    // Enumerate all active displays
    bool EnumerateDisplays();

    // Get list of enumerated displays
    const std::vector<DisplayInfo>& GetDisplays() const { return m_displays; }

    // Create a borderless full-screen window on the target monitor index (default index 1 for 2nd monitor)
    HWND CreateWindowOnMonitor(int targetMonitorIndex, const wchar_t* windowTitle);

    // Get created window handle
    HWND GetHWND() const { return m_hwnd; }

    // Get dimensions of current window
    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }
    int GetRefreshRate() const { return m_refreshRate; }

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    static BOOL CALLBACK MonitorEnumProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData);

    std::vector<DisplayInfo> m_displays;
    HWND m_hwnd;
    HINSTANCE m_hInstance;
    int m_width;
    int m_height;
    int m_refreshRate;
};

#endif // DISPLAY_MANAGER_H
