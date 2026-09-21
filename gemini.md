# Projection Engine - Context & Project Tracker

## Project overview
Projection Engine is a C++ Win32 application for projecting calibrated content onto real surfaces, walls, or secondary displays. It loads animation scripts, defines geometric areas, supports calibration, and renders visual sequences in real time.

The project is intentionally structured around a clean split between source code and headers, so the runtime logic and the public interfaces stay organized as the project grows.

---

## Current status
The project is in active development and already includes:
- multi-monitor handling for projector-style output
- area-based calibration and geometry editing
- render pipeline with GDI/GDI+ integration
- text-based animation scripting via `.txt` files
- support for image and video playback in the animation flow
- a modern folder layout under `source/` and `include/`

---

## Project structure
```text
projection_engine/
├── animations/         # Scene scripts (.txt)
├── build/              # Local build output (ignored by git)
├── config/             # Project config and calibration data
├── fonts/              # Custom fonts used by the renderer
├── images/             # Assets used by the projection engine
├── include/            # Public headers
│   ├── anim/
│   ├── app/
│   ├── calib/
│   ├── display/
│   ├── media/
│   ├── model/
│   ├── render/
│   └── util/
├── source/             # C++ implementation files
│   ├── anim/
│   ├── app/
│   ├── calib/
│   ├── display/
│   ├── media/
│   ├── model/
│   ├── render/
│   └── main.cpp
├── third_party/         # Vendored dependencies
├── CMakeLists.txt      # Build configuration
├── README.md           # Main project documentation
├── gemini.md           # Project tracker for AI context
├── .gitignore          # Local build artifacts only
└── .git/
```

---

## Build and run

### Build
```powershell
cmake -B build -G Ninja
cmake --build build
```

### Run
```powershell
.\build\PatronAnimation.exe 1
```

Examples:
```powershell
.\build\PatronAnimation.exe 0
.\build\PatronAnimation.exe 0 --calibrate
.\build\PatronAnimation.exe 0 --anim animations\show_espectacular.txt
```

---

## Important notes
- The build folder is intentionally local-only and should not be committed.
- The project currently uses Windows-specific APIs, so it is intended for native Windows builds.
- Animated behavior is driven from `.txt` files, which keeps the content easy to edit without recompiling the engine.
- The image/video names used by animation scripts should match the actual files in `images/`.

---

## Next steps
1. Keep the source/include split consistent as features grow.
2. Maintain a clean naming convention for assets and animation references.
3. Validate runtime behavior on the target projector/display.
4. Expand the animation script examples as the system evolves.

3. **Stage 3: Animation Modules (`IAnimationModule`)**:
   - Abstract animation interface `IAnimationModule` (`Initialize`, `Update`, `Render`).
   - Implement animation drivers (e.g. `AreaBlinkModule`, `PulseModule`, `SequenceModule`, `ColorFadeModule`).
   - Sequencer / Timeline controller to chain animations across areas.
   - The reserved `opacity`, `isBlinking`, `blinkRate` and `phase` fields on
     `ProjectionArea` are the intended hooks.

### Known gaps / deliberate omissions
- **No resolution scaling.** Coordinates are absolute pixels for the canvas in
  the config; a different monitor logs a warning and needs recalibrating.
- **No reference image underlay** during calibration: GDI alone cannot decode
  PNG, that would need WIC or GDI+.
- **No vsync.** `BitBlt` onto a persistent `GetDC` outside `WM_PAINT` can tear
  despite the double buffer.
- **Show mode still redraws at 60 FPS** even though the mask is now static.
  Harmless but wasteful; a dirty-flag render would need care to avoid stale
  frames when the window is damaged.

### Where the tunable values live
Grouped on purpose, so behaviour can be changed without hunting through code:
- `CalibrationController.h`, `--- Tuning ---` block: nudge step sizes, undo
  depth, nudge-coalescing window, corner-handle blink period.
- `Application.cpp`, anonymous namespace: target frame rate, overlay palette
  and overlay layout in pixels.
- `PatternGrid.cpp`, anonymous namespace: size of an area created with `N`,
  offset of a copy made with `D`.
- `RenderEngine.cpp`, anonymous namespace: HUD font and padding, halftone
  pattern bits.
- `config/pattern_config.json`: all geometry. Never in code.

