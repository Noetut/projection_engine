# Projection Engine Wiki

Comprehensive architecture, scripting, calibration, and developer reference for **Projection Engine**.

---

## 1. System Overview & Architecture

**Projection Engine** is a native Win32/C++17 hardware-accelerated projection mapping and animation engine. It bridges real physical surfaces and 2D projections via live quad-based perspective calibration, high-performance GDI/GDI+ rendering locked to display refresh rate (60–360 FPS), Media Foundation video streaming, and plaintext animation scripting.

```
                          ┌───────────────────────────┐
                          │   main.cpp / Application  │
                          └─────────────┬─────────────┘
                                        │
           ┌────────────────────────────┼────────────────────────────┐
           ▼                            ▼                            ▼
┌─────────────────────┐      ┌─────────────────────┐      ┌─────────────────────┐
│   DisplayManager    │      │  AnimationController│      │CalibrationController│
│ (Monitor & Window)  │      │(Timeline & Playback)│      │  (Quad Calibration) │
└─────────────────────┘      └──────────┬──────────┘      └──────────┬──────────┘
                                        │                            │
                                        ▼                            ▼
                             ┌─────────────────────┐      ┌─────────────────────┐
                             │     VideoPlayer     │      │     PatternGrid     │
                             │ (Media Foundation)  │      │  (Quads & Config)   │
                             └──────────┬──────────┘      └──────────┬──────────┘
                                        │                            │
                                        ▼                            ▼
                             ┌──────────────────────────────────────────────────┐
                             │                   RenderEngine                   │
                             │        (Double Buffer, GDI+, Alpha HUD)          │
                             └──────────────────────────────────────────────────┘
```

---

## 2. Directory Layout & File Organization

The project employs a flat file organization without subfolders for both headers and source implementations:

```text
projection_engine/
├── animations/              # Plain text animation sequences (*.txt)
├── build/                   # Local build outputs and binaries (git-ignored)
├── config/                  # pattern_config.json (quads, canvas resolution)
├── fonts/                   # TTF font files (e.g., Bebas Neue Cyrillic)
├── images/                  # Media assets and warning placeholder (EMPTY.png)
│   └── EMPTY.png            # Visual fallback warning for missing media
├── include/                 # Flat public C++ header files (*.h)
│   ├── AnimationController.h
│   ├── AnimationParser.h
│   ├── AnimationTypes.h
│   ├── Application.h
│   ├── CalibrationController.h
│   ├── DisplayManager.h
│   ├── Geometry.h
│   ├── PatternConfig.h
│   ├── PatternGrid.h
│   ├── ProjectionArea.h
│   ├── RenderEngine.h
│   ├── StringUtil.h
│   └── VideoPlayer.h
├── src/                     # Flat C++ source implementation files (*.cpp)
│   ├── AnimationController.cpp
│   ├── AnimationParser.cpp
│   ├── Application.cpp
│   ├── CalibrationController.cpp
│   ├── DisplayManager.cpp
│   ├── PatternConfig.cpp
│   ├── PatternGrid.cpp
│   ├── RenderEngine.cpp
│   ├── VideoPlayer.cpp
│   └── main.cpp
├── third_party/             # Vendored headers
│   └── nlohmann/json.hpp    # Modern JSON for C++
├── CMakeLists.txt           # CMake build definition (C++17)
├── README.md                # Project overview and quick start
└── WIKI.md                  # Complete technical reference manual
```

---

## 3. Core Engine Components

### 3.1 `Application` (`Application.h` / `Application.cpp`)
* Main entry point and orchestration loop.
* Runs the high-resolution message pump locked to the target display's refresh rate using `timeBeginPeriod(1)`.
* Toggles between **Show Mode** and **Calibration Mode** (<kbd>F1</kbd>).

### 3.2 `DisplayManager` (`DisplayManager.h` / `DisplayManager.cpp`)
* Enumerates physical monitors via Win32 `EnumDisplayMonitors`.
* Creates a borderless popup window precisely matching the resolution and virtual coordinates of the target projector or monitor.

### 3.3 `RenderEngine` (`RenderEngine.h` / `RenderEngine.cpp`)
* Manages an offscreen 32-bit double buffer (`m_memDC`).
* **Hardware-accelerated drawing**: Solid quad fills, halftone patterns, custom TTF font rendering with anti-aliasing via GDI+.
* **Text Auto-Fit**: Intelligently measures text dimensions against the quad bounding box (`maxAllowedW` and `maxAllowedH`) and auto-scales down font point sizes via binary search so text never overflows.
* **Asset Fallback**: When an image or background video fails to load, `RenderEngine` automatically draws `images/EMPTY.png` (centered with letterbox black bars for background, or fitted within the quad for area images) to alert the user immediately.

### 3.4 `AnimationController` & `AnimationParser` (`AnimationController.h, cpp`, `AnimationParser.h, cpp`)
* Parses human-readable `.txt` scripts into structured frame sequences.
* Handles frame timers, looping, step durations, mouse clicks, and palette pulsations.
* Supports seamless slide transitions (`WAIT_CLICK`) where video playback loops continuously in the background until user advances.

### 3.5 `VideoPlayer` (`VideoPlayer.h` / `VideoPlayer.cpp`)
* Asynchronous hardware-accelerated video decoding using Microsoft Media Foundation (`IMFSourceReader`).
* Background decoding worker thread keeps the main render loop locked at 60 FPS without frame drops.
* Seamless looping with timestamp synchronization to avoid seam stutter.

### 3.6 `CalibrationController` & `PatternGrid` (`CalibrationController.h, cpp`, `PatternGrid.h, cpp`)
* Maintains the list of projection areas (`ProjectionArea`).
* Interactive keyboard and mouse corner manipulation with 50 levels of undo (`Ctrl+Z`).
* Serializes canvas dimensions and calibrated quads directly to `config/pattern_config.json`.

---

## 4. Animation Scripting Reference

Animation scripts are stored in `animations/*.txt`.

### 4.1 Header Directives
Placed at the very top of the script:
```text
name: Presentation Name        # Sequence title
loop: false                    # Automatically loop sequence when it ends
default_step: 0.2s             # Default step duration if not specified
reset_images: true             # Clear all images and texts on sequence start/loop
preload_videos: video1, video2 # Pre-decode video headers at engine startup
preload_images: img1, img2     # Pre-cache images into RAM
```

### 4.2 Step Timing & Compound Actions
Each step begins with a duration (`ms` or `s`), followed by one or more commands separated by `&`:
```text
# Wait 0.5s, clear everything, stop video
0.5s CLEAR_IMAGES & CLEAR_TEXT ALL & STOP_VIDEO & ALL_OFF

# Advance immediately or pause on click
BG_VIDEO presentation_intro NOFADE & TEXT 10 "Bebas Neue Cyrillic" 210 bold "TITLE" & WAIT_CLICK
```

### 4.3 Command Reference Table

| Command | Arguments | Description |
| :--- | :--- | :--- |
| `ALL_ON` | — | Lights up all calibrated quads. |
| `ALL_OFF` | — | Turns off all quads (renders black). |
| `ON` | `<id ...>` | Turns on specific quad ID(s) (e.g. `ON 1 2 3`). |
| `OFF` | `<id ...>` | Turns off specific quad ID(s). |
| `TOGGLE` | `<id ...>` | Inverts current visibility state. |
| `BG_VIDEO` | `<name> [NOFADE]` | Starts playing background video. `NOFADE` prevents black fade transitions. |
| `STOP_VIDEO`| — | Stops and clears active background video. |
| `TEXT` | `<id> [font] [size] [style] "<text>"` | Displays centered vector text inside quad ID. Text automatically scales if oversized. |
| `CLEAR_TEXT`| `<id>` or `ALL` | Clears text from specific quad or all quads. |
| `IMAGE` | `<id> <image_name>` | Renders image centered and cropped inside quad ID. |
| `CLEAR_IMAGE`| `<id>` or `ALL` | Removes image and restores solid color. |
| `CLEAR_IMAGES`| — | Clears images on all quads simultaneously. |
| `WAIT` | `<duration>` | Pauses animation timeline for specified time (e.g. `WAIT 1.5s`). |
| `WAIT_CLICK`| — | Pauses timeline on current frame until mouse click or <kbd>Enter</kbd>. |
| `PALPITATE` | `<id> [mode] [freq]` | Smoothly pulses brightness, two-color oscillation, or rainbow cycle. |
| `STOP_PALPITATE`| `<id>` or `ALL` | Stops active pulsations. |

---

## 5. Controls & Keyboard Shortcuts

### Show & Interactive Mode
* <kbd>Left Click</kbd> / <kbd>Enter</kbd>: Advance to next slide / trigger cue.
* <kbd>Space</kbd>: Play / Pause animation.
* <kbd>R</kbd>: Restart animation from frame 1.
* <kbd>Tab</kbd>: Switch to next animation file in `animations/`.
* <kbd>0</kbd>–<kbd>9</kbd>: Toggle visibility of area by ID.
* <kbd>A</kbd> / <kbd>O</kbd>: Turn ALL areas ON / OFF.
* <kbd>F1</kbd>: Enter Calibration Mode.
* <kbd>Esc</kbd>: Exit application.

### Calibration Mode (<kbd>F1</kbd>)
* <kbd>Tab</kbd> / <kbd>Shift</kbd>+<kbd>Tab</kbd>: Cycle through areas.
* <kbd>0</kbd>–<kbd>9</kbd>: Select area directly by ID.
* <kbd>Q</kbd> / <kbd>E</kbd>: Cycle active corner (Top-Left, Top-Right, Bottom-Right, Bottom-Left).
* Numpad <kbd>7</kbd> <kbd>9</kbd> <kbd>3</kbd> <kbd>1</kbd>: Select specific corner directly.
* Numpad <kbd>5</kbd>: Select entire area (moves all 4 corners simultaneously).
* **Arrow keys**: Nudge by 1 px (<kbd>Shift</kbd>: 10 px, <kbd>Ctrl+Shift</kbd>: 50 px).
* <kbd>N</kbd> / <kbd>D</kbd>: Create New quad / Duplicate selected area.
* <kbd>T</kbd> / <kbd>Shift</kbd>+<kbd>T</kbd>: Create Text box / Toggle area type (quad ↔ text).
* <kbd>Shift</kbd>+<kbd>Delete</kbd>: Delete selected area.
* <kbd>Ctrl+Z</kbd>: Undo calibration action.
* <kbd>Ctrl+S</kbd>: Save geometry directly to `config/pattern_config.json`.
* <kbd>H</kbd>: Toggle on-screen HUD panel.
* <kbd>F1</kbd>: Return to Show Mode.

---

## 6. Build Instructions

### Prerequisites
* Windows 10/11
* CMake 3.16 or newer
* MSVC (Visual Studio 2019/2022) or MinGW-w64 with C++17 support

### Building with CMake
```powershell
# Generate build configuration
cmake -B build

# Compile Release binary
cmake --build build --config Release
```

The resulting executable `PatronAnimation.exe` is generated in `build/` (or `build/Release/`).
