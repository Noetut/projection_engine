#ifndef ANIMATION_CONTROLLER_H
#define ANIMATION_CONTROLLER_H

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "AnimationTypes.h"
#include "media/VideoPlayer.h"

class PatternGrid;
class RenderEngine;

class AnimationController {
public:
    AnimationController();
    ~AnimationController();

    bool LoadFromFile(const std::string& filePath, std::string& outError);
    bool LoadFromSequence(const AnimationSequence& sequence);

    // Preloads both images and videos into memory
    void PreloadMedia(RenderEngine& renderEngine);
    void PreloadImages(RenderEngine& renderEngine) { PreloadMedia(renderEngine); }

    void Play();
    void Pause();
    void TogglePlayPause();
    void Stop();
    void Restart(PatternGrid& grid);

    void TriggerClick(PatternGrid& grid);

    void Update(double deltaTime, PatternGrid& grid);

    bool IsPlaying() const { return m_isPlaying; }
    bool IsWaitingForClick() const { return m_isWaitingForClick; }
    bool HasSequence() const { return !m_sequence.Empty(); }
    size_t CurrentFrameIndex() const { return m_currentFrameIndex; }
    size_t TotalFrames() const { return m_sequence.FrameCount(); }
    const std::string& SequenceName() const { return m_sequence.name; }
    const std::string& FilePath() const { return m_filePath; }

    bool HasBackgroundVideo() const {
        return m_activeVideoPlayer && m_activeVideoPlayer->IsPlaying() && m_activeVideoPlayer->HasFrame();
    }
    const BYTE* GetBackgroundVideoFrame(int& outW, int& outH) const {
        if (!m_activeVideoPlayer || !m_activeVideoPlayer->HasFrame() || !m_activeVideoPlayer->IsPlaying()) return nullptr;
        outW = m_activeVideoPlayer->GetWidth();
        outH = m_activeVideoPlayer->GetHeight();
        return m_activeVideoPlayer->GetFrameData();
    }
    float GetBackgroundVideoBrightness() const {
        return m_activeVideoPlayer ? m_activeVideoPlayer->GetBrightness() : 1.0f;
    }

private:
    void ApplyFrame(const AnimationFrame& frame, PatternGrid& grid);

    enum class PalpitateMode {
        Brightness, // Breathe brightness of a base color (default white)
        TwoColors,  // Smoothly oscillate between colorA and colorB
        Rainbow     // Continuously cycle through full rainbow hues
    };

    struct ActivePalpitation {
        std::vector<int> areaIndices;
        PalpitateMode mode = PalpitateMode::Brightness;
        COLORREF colorA = RGB(255, 255, 255);
        COLORREF colorB = RGB(255, 255, 255);
        float minBrightness = 0.5f;
        float maxBrightness = 1.0f;
        float frequency = 1.2f;
        double initialPhase = 0.0;
        double timer = 0.0;
        bool hasCustomBrightness = false;
    };

    COLORREF ComputePalpitationColor(const ActivePalpitation& p, double timer) const;
    void UpdatePalpitations(double deltaTime, PatternGrid& grid);
    void ClearPalpitations(PatternGrid& grid);
    void RemovePalpitationsForArea(int areaIndex, PatternGrid& grid);
    bool PreloadVideo(const std::string& videoPath);

    std::unordered_map<std::string, std::unique_ptr<VideoPlayer>> m_videoPlayers;
    VideoPlayer*      m_activeVideoPlayer;
    AnimationSequence m_sequence;
    std::string       m_filePath;
    size_t            m_currentFrameIndex;
    double            m_frameTimer;
    bool              m_isPlaying;
    bool              m_isWaitingForClick;
    bool              m_frameApplied;
    std::vector<ActivePalpitation> m_activePalpitations;
};

#endif // ANIMATION_CONTROLLER_H
