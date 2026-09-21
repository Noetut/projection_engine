#ifndef VIDEO_PLAYER_H
#define VIDEO_PLAYER_H

#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>

#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

class VideoPlayer {
public:
    VideoPlayer();
    ~VideoPlayer();

    // Lifecycle
    bool Initialize();
    void Shutdown();

    // Video file control
    // Preloads the video in memory, extracts properties and decodes first frame, but stays paused
    bool Preload(const std::wstring& filePath, bool loop = true);
    // Opens and starts playing immediately (uses preloaded state if already loaded)
    bool Open(const std::wstring& filePath, bool loop = true);
    void Close();

    void Play();
    void Pause();
    void Stop();

    // Advances playback according to elapsed time (instant, non-blocking)
    void Update(double deltaTime);

    // Frame access
    bool IsPlaying() const { return m_isPlaying; }
    bool HasFrame() const { return m_hasFrame; }
    bool IsLoaded() const { return m_reader != NULL && m_hasFrame; }
    const std::wstring& GetLoadedPath() const { return m_currentFilePath; }
    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }
    const BYTE* GetFrameData() const;

    // Fade / Shadeoff control
    void SetFadeDuration(double seconds) { m_fadeDuration = (seconds >= 0.0) ? seconds : 0.0; }
    double GetFadeDuration() const { return m_fadeDuration; }
    float GetBrightness() const { return m_currentBrightness.load(); }
    double GetDuration() const { return m_duration; }
    double GetCurrentTime() const { return m_currentTime; }

private:
    bool SetupSourceReader(const std::wstring& filePath);
    void WorkerLoop();
    void StartWorker();
    void StopWorker();
    bool DecodeNextSample(std::vector<BYTE>& outBuffer);

    IMFSourceReader* m_reader;
    bool m_mfInitialized;
    std::wstring m_currentFilePath;
    bool m_loop;

    int m_width;
    int m_height;
    double m_frameDuration;
    double m_timeAccumulator;

    // Duration, playback position and shadeoff/fade tracking
    double m_duration;
    double m_currentTime;
    double m_fadeDuration;
    std::atomic<float> m_currentBrightness;
    LONGLONG m_lastSampleTimestamp;
    std::atomic<bool> m_loopOccurred;

    std::atomic<bool> m_isPlaying;
    std::atomic<bool> m_hasFrame;

    // Background decoding thread
    std::thread m_workerThread;
    std::atomic<bool> m_stopWorker;
    std::atomic<bool> m_workerActive;
    std::mutex m_queueMutex;
    std::condition_variable m_cvWorker;
    std::condition_variable m_cvMain;

    // Decoded frames queue and memory pool to avoid allocations
    std::deque<std::vector<BYTE>> m_frameQueue;
    std::vector<std::vector<BYTE>> m_freeBuffers;
    std::vector<BYTE> m_displayBuffer;
    mutable std::mutex m_displayMutex;

    static const size_t kMaxQueueSize = 3;
};

#endif // VIDEO_PLAYER_H
