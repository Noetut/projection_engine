#ifndef ANIMATION_TYPES_H
#define ANIMATION_TYPES_H

#include <windows.h>
#include <string>
#include <vector>

enum class ActionType {
    AllOn,
    AllOff,
    TurnOn,
    TurnOff,
    Toggle,
    SetColor,
    SetMask,
    SetImage,
    ClearImage,
    ClearAllImages,
    PreloadImage,
    SetText,
    ClearText,
    ClearAllTexts,
    SetBackgroundVideo,
    StopBackgroundVideo,
    Palpitate,
    StopPalpitate
};

struct AnimationAction {
    ActionType  type = ActionType::AllOn;
    int         targetIndex = -1;   // 0-based area index (-1 if unused)
    int         targetId = -1;      // Area id from config (-1 if unused)
    std::string targetName;         // Optional area name if referenced by name
    std::string imagePath;          // Image path / name for SetImage / Preload
    std::string videoPath;          // Video path / name for SetBackgroundVideo
    std::string text;               // Text content for SetText
    std::string fontFace = "Arial"; // Font family name
    int         fontSize = 32;      // Font size in points
    COLORREF    textColor = RGB(255, 255, 255); // Color of text glyphs
    COLORREF    color = RGB(255, 255, 255);     // Base fill color for TurnOn, SetColor, AllOn
    std::vector<COLORREF> colors;               // Colors for Palpitate (1 = single color breath, 2 = two-color oscillation)
    bool        isRainbow = false;              // True for continuous rainbow hue cycling
    bool        hasCustomBrightness = false;    // True if min/max % was explicitly set
    std::vector<bool> mask;         // Used for SetMask
    std::vector<int> targetIds;     // Multiple target IDs for bulk commands like Palpitate
    float       minBrightness = 0.5f;   // Min brightness for Palpitate (0.0 to 1.0)
    float       startBrightness = 1.0f; // Initial brightness at start of palpitation (0.0 to 1.0)
    float       maxBrightness = 1.0f;   // Max brightness for Palpitate (0.0 to 1.0)
    bool        startFalling = true;    // If true, start on the falling slope
    float       frequency = 1.2f;       // Frequency in Hz for Palpitate (smooth breathing pace)
    float       initialPhase = 0.0f;    // Custom phase in radians
    bool        hasCustomPhase = false; // True if custom phase was explicitly set
    double      fadeDuration = 1.0;     // Fade/shadeoff duration in seconds for SetBackgroundVideo (default 1.0s)
};

struct AnimationFrame {
    double duration = 0.3;          // Duration in seconds to wait before next frame
    bool   hasExplicitDuration = false; // True if duration was set explicitly (e.g. "0.5s" or "WAIT 1")
    bool   waitForClick = false;    // If true, pauses at this frame waiting for mouse click / cue
    std::string segmentName;        // Optional cue/segment name
    std::vector<AnimationAction> actions;
};

struct AnimationSequence {
    std::string name;
    bool        loop = true;
    bool        resetImages = true; // Clear image overlays to default white on startup
    double      defaultStep = 0.3;  // Default step duration in seconds
    std::vector<std::string>    preloadImages; // Explicitly preloaded image list
    std::vector<std::string>    preloadVideos; // Explicitly preloaded video list
    std::vector<AnimationFrame> frames;

    bool Empty() const { return frames.empty(); }
    size_t FrameCount() const { return frames.size(); }
};

#endif // ANIMATION_TYPES_H
