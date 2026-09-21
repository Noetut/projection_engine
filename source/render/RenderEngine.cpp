#include "RenderEngine.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <set>
#include <vector>
#include <sstream>

#include "util/StringUtil.h"

namespace {

// 8x8 monochrome checkerboard. With a monochrome pattern brush GDI paints the
// 0 bits in the DC text colour and the 1 bits in the DC background colour,
// which is what gives the selected area its half-lit look.
const WORD kHalftoneBits[8] = {
    0xAAAA, 0x5555, 0xAAAA, 0x5555, 0xAAAA, 0x5555, 0xAAAA, 0x5555
};
const int kHalftoneSize = 8;   // Pixels per side of the pattern bitmap

// HUD font. Fixed pitch keeps the coordinate columns in the panel aligned; the
// negative height asks for a character height rather than a cell height.
const int      kHudFontHeight = -14;
const wchar_t* kHudFontFace   = L"Consolas";

// Black margin painted around HUD text so it stays readable over a lit area.
const int kHudTextPadding = 6;

std::string MakeQuadKey(const Quad& quad) {
    std::ostringstream ss;
    for (int i = 0; i < 4; ++i) {
        ss << quad.corners[i].x << "," << quad.corners[i].y << ";";
    }
    return ss.str();
}

} // namespace

RenderEngine::RenderEngine()
    : m_hwnd(NULL)
    , m_hdc(NULL)
    , m_memDC(NULL)
    , m_hBitmap(NULL)
    , m_hOldBitmap(NULL)
    , m_blackBrush(NULL)
    , m_whiteBrush(NULL)
    , m_halftoneBrush(NULL)
    , m_hudFont(NULL)
    , m_width(0)
    , m_height(0)
    , m_scratchDC(NULL)
    , m_scratchBitmap(NULL)
    , m_scratchOldBitmap(NULL)
    , m_scratchBits(NULL)
    , m_scratchWidth(0)
    , m_scratchHeight(0)
    , m_blackDC(NULL)
    , m_blackBitmap(NULL)
    , m_blackOldBitmap(NULL)
    , m_gdiplusToken(0)
    , m_fontCollection(nullptr)
{
}

RenderEngine::~RenderEngine() {
    Cleanup();
}

// ---------------------------------------------------------------------------
// Resource lifecycle
// ---------------------------------------------------------------------------

bool RenderEngine::Initialize(HWND hwnd, int width, int height) {
    Cleanup();

    m_hwnd = hwnd;
    m_width = width;
    m_height = height;

    m_hdc = GetDC(m_hwnd);
    if (!m_hdc) {
        std::cerr << "[RenderEngine] ERROR: Could not get device context." << std::endl;
        return false;
    }

    m_memDC = CreateCompatibleDC(m_hdc);
    m_hBitmap = CreateCompatibleBitmap(m_hdc, m_width, m_height);
    m_hOldBitmap = (HBITMAP)SelectObject(m_memDC, m_hBitmap);

    // Cache common GDI objects for high-performance rendering without leaks
    m_blackBrush = CreateSolidBrush(RGB(0, 0, 0));
    m_whiteBrush = CreateSolidBrush(RGB(255, 255, 255));

    HBITMAP halftoneBitmap = CreateBitmap(kHalftoneSize, kHalftoneSize, 1, 1, kHalftoneBits);
    if (halftoneBitmap) {
        m_halftoneBrush = CreatePatternBrush(halftoneBitmap);
        DeleteObject(halftoneBitmap);
    }

    m_hudFont = CreateFontW(kHudFontHeight, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                            CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, kHudFontFace);

    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    Gdiplus::GdiplusStartup(&m_gdiplusToken, &gdiplusStartupInput, NULL);

    m_fontCollection = new Gdiplus::PrivateFontCollection();
    LoadCustomFonts();

    std::cout << "[RenderEngine] Initialized offscreen double buffer ("
              << m_width << "x" << m_height << ")" << std::endl;
    return true;
}

void RenderEngine::ClearQuadCache() {
    for (auto& pair : m_quadImageCache) {
        if (pair.second.hdc) {
            if (pair.second.hOldBitmap) SelectObject(pair.second.hdc, pair.second.hOldBitmap);
            DeleteDC(pair.second.hdc);
        }
        if (pair.second.hBitmap) DeleteObject(pair.second.hBitmap);
    }
    m_quadImageCache.clear();

    for (auto& pair : m_quadTextCache) {
        if (pair.second.hdc) {
            if (pair.second.hOldBitmap) SelectObject(pair.second.hdc, pair.second.hOldBitmap);
            DeleteDC(pair.second.hdc);
        }
        if (pair.second.hBitmap) DeleteObject(pair.second.hBitmap);
    }
    m_quadTextCache.clear();
}

void RenderEngine::Cleanup() {
    ClearQuadCache();

    for (auto& pair : m_brushCache) {
        if (pair.second) DeleteObject(pair.second);
    }
    m_brushCache.clear();

    for (auto& cached : m_pens) {
        if (cached.pen) DeleteObject(cached.pen);
    }
    m_pens.clear();

    if (m_hudFont) {
        DeleteObject(m_hudFont);
        m_hudFont = NULL;
    }

    if (m_halftoneBrush) {
        DeleteObject(m_halftoneBrush);
        m_halftoneBrush = NULL;
    }

    if (m_blackBrush) {
        DeleteObject(m_blackBrush);
        m_blackBrush = NULL;
    }

    if (m_whiteBrush) {
        DeleteObject(m_whiteBrush);
        m_whiteBrush = NULL;
    }

    if (m_memDC) {
        if (m_hOldBitmap) {
            SelectObject(m_memDC, m_hOldBitmap);
            m_hOldBitmap = NULL;
        }
        DeleteDC(m_memDC);
        m_memDC = NULL;
    }

    if (m_hBitmap) {
        DeleteObject(m_hBitmap);
        m_hBitmap = NULL;
    }

    if (m_hdc && m_hwnd) {
        ReleaseDC(m_hwnd, m_hdc);
        m_hdc = NULL;
    }

    if (m_scratchDC) {
        if (m_scratchOldBitmap) {
            SelectObject(m_scratchDC, m_scratchOldBitmap);
            m_scratchOldBitmap = NULL;
        }
        DeleteDC(m_scratchDC);
        m_scratchDC = NULL;
    }

    if (m_scratchBitmap) {
        DeleteObject(m_scratchBitmap);
        m_scratchBitmap = NULL;
    }

    m_scratchBits = NULL;
    m_scratchWidth = 0;
    m_scratchHeight = 0;

    if (m_blackDC) {
        if (m_blackOldBitmap) {
            SelectObject(m_blackDC, m_blackOldBitmap);
            m_blackOldBitmap = NULL;
        }
        DeleteDC(m_blackDC);
        m_blackDC = NULL;
    }

    if (m_blackBitmap) {
        DeleteObject(m_blackBitmap);
        m_blackBitmap = NULL;
    }

    for (auto& pair : m_imageCache) {
        if (pair.second) delete pair.second;
    }
    m_imageCache.clear();

    if (m_fontCollection) {
        delete m_fontCollection;
        m_fontCollection = nullptr;
    }

    if (m_gdiplusToken) {
        Gdiplus::GdiplusShutdown(m_gdiplusToken);
        m_gdiplusToken = 0;
    }
}

void RenderEngine::LoadCustomFonts() {
    std::vector<std::wstring> searchDirs = {
        L"fonts",
        L"..\\fonts",
        L"..\\..\\fonts"
    };

    wchar_t exeBuf[MAX_PATH] = { 0 };
    DWORD written = GetModuleFileNameW(NULL, exeBuf, MAX_PATH);
    if (written > 0 && written < MAX_PATH) {
        std::wstring exePath(exeBuf);
        size_t slash = exePath.find_last_of(L"\\/");
        if (slash != std::wstring::npos) {
            std::wstring exeDir = exePath.substr(0, slash);
            searchDirs.push_back(exeDir + L"\\fonts");
            searchDirs.push_back(exeDir + L"\\..\\fonts");
            searchDirs.push_back(exeDir + L"\\..\\..\\fonts");
        }
    }

    std::set<std::wstring> loadedFiles;

    for (const auto& dir : searchDirs) {
        std::wstring pattern = dir + L"\\*.*";
        WIN32_FIND_DATAW ffd;
        HANDLE hFind = FindFirstFileW(pattern.c_str(), &ffd);
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (!(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                    std::wstring fname = ffd.cFileName;
                    std::wstring ext;
                    size_t dot = fname.rfind(L'.');
                    if (dot != std::wstring::npos) {
                        ext = fname.substr(dot);
                        for (auto& c : ext) c = towlower(c);
                    }
                    if (ext == L".ttf" || ext == L".otf") {
                        std::wstring lowerName = fname;
                        for (auto& c : lowerName) c = towlower(c);
                        if (loadedFiles.insert(lowerName).second) {
                            std::wstring fullPath = dir + L"\\" + fname;
                            AddFontResourceExW(fullPath.c_str(), FR_PRIVATE, 0);
                            if (m_fontCollection) {
                                Gdiplus::Status st = m_fontCollection->AddFontFile(fullPath.c_str());
                                if (st == Gdiplus::Ok) {
                                    std::cout << "[RenderEngine] Loaded custom font: " << WideToUtf8(fullPath) << std::endl;
                                }
                            }
                        }
                    }
                }
            } while (FindNextFileW(hFind, &ffd));
            FindClose(hFind);
        }
    }
}

void RenderEngine::Resize(int width, int height) {
    if (width != m_width || height != m_height) {
        Initialize(m_hwnd, width, height);
    }
}

// ---------------------------------------------------------------------------
// Frame and show-mode output
// ---------------------------------------------------------------------------

void RenderEngine::BeginFrame() {
    // Ready memory DC for drawing
}

void RenderEngine::EndFrame() {
    if (!m_hdc || !m_memDC) return;

    // Fast bit-block transfer from offscreen buffer to window DC
    BitBlt(m_hdc, 0, 0, m_width, m_height, m_memDC, 0, 0, SRCCOPY);
}

void RenderEngine::RenderBlack() {
    if (!m_memDC) return;

    RECT rect = { 0, 0, m_width, m_height };
    FillRect(m_memDC, &rect, m_blackBrush ? m_blackBrush : (HBRUSH)GetStockObject(BLACK_BRUSH));
}

void RenderEngine::DrawBackgroundVideo(const BYTE* pixels, int videoWidth, int videoHeight, float brightness) {
    if (!pixels || videoWidth <= 0 || videoHeight <= 0 || !m_memDC) return;

    if (brightness <= 0.001f) {
        RenderBlack();
        return;
    }

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = videoWidth;
    bmi.bmiHeader.biHeight = -videoHeight; // Negative for top-down DIB
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    SetStretchBltMode(m_memDC, COLORONCOLOR);
    StretchDIBits(m_memDC,
                  0, 0, m_width, m_height,
                  0, 0, videoWidth, videoHeight,
                  pixels, &bmi, DIB_RGB_COLORS, SRCCOPY);

    if (brightness < 0.999f) {
        EnsureBlackBitmap();
        if (m_blackDC) {
            BLENDFUNCTION bf = {};
            bf.BlendOp = AC_SRC_OVER;
            bf.BlendFlags = 0;
            bf.SourceConstantAlpha = static_cast<BYTE>(std::clamp((1.0f - brightness) * 255.0f, 0.0f, 255.0f));
            bf.AlphaFormat = 0;

            AlphaBlend(m_memDC, 0, 0, m_width, m_height,
                       m_blackDC, 0, 0, 16, 16, bf);
        }
    }
}

void RenderEngine::RenderAreas(const std::vector<ProjectionArea>& areas,
                               const BYTE* bgVideoPixels, int bgVideoWidth, int bgVideoHeight,
                               float bgVideoBrightness) {
    if (!m_memDC) return;

    if (bgVideoPixels && bgVideoWidth > 0 && bgVideoHeight > 0) {
        DrawBackgroundVideo(bgVideoPixels, bgVideoWidth, bgVideoHeight, bgVideoBrightness);
    } else {
        RenderBlack();
    }

    for (const auto& area : areas) {
        if (!area.isVisible) continue;

        // Text projection area: render text only, absolutely NO background fill!
        if (area.type == "text" || !area.text.empty()) {
            if (!area.text.empty()) {
                DrawQuadText(area.quad, area.text, area.fontFace, area.fontSize, area.textColor);
            }
            continue;
        }

        if (!area.imagePath.empty()) {
            Gdiplus::Bitmap* bmp = GetOrLoadImage(area.imagePath);
            if (bmp) {
                DrawQuadImage(area.quad, bmp, area.imagePath);
                continue;
            }
        }

        FillQuad(area.quad, area.color);
    }
}

Gdiplus::Bitmap* RenderEngine::GetOrLoadImage(const std::string& path) {
    if (path.empty() || path == "none" || path == "clear") return nullptr;

    auto it = m_imageCache.find(path);
    if (it != m_imageCache.end()) {
        return it->second;
    }

    std::vector<std::string> candidates;
    candidates.push_back(path);
    candidates.push_back(path + ".jpg");
    candidates.push_back(path + ".png");
    candidates.push_back(path + ".jpeg");
    candidates.push_back("images/" + path);
    candidates.push_back("images/" + path + ".jpg");
    candidates.push_back("images/" + path + ".png");
    candidates.push_back("images/" + path + ".jpeg");
    candidates.push_back("../images/" + path);
    candidates.push_back("../images/" + path + ".jpg");
    candidates.push_back("../images/" + path + ".png");
    candidates.push_back("../images/" + path + ".jpeg");
    candidates.push_back("../../images/" + path);
    candidates.push_back("../../images/" + path + ".jpg");
    candidates.push_back("../../images/" + path + ".png");

    std::wstring foundPath;
    for (const auto& cand : candidates) {
        std::wstring wide = AnsiToWide(cand);
        DWORD attr = GetFileAttributesW(wide.c_str());
        if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
            foundPath = wide;
            break;
        }
    }

    if (foundPath.empty()) {
        std::cerr << "[RenderEngine] ERROR: Could not locate image for '" << path << "'." << std::endl;
        m_imageCache[path] = nullptr;
        return nullptr;
    }

    Gdiplus::Bitmap* bitmap = Gdiplus::Bitmap::FromFile(foundPath.c_str());
    if (!bitmap || bitmap->GetLastStatus() != Gdiplus::Ok) {
        std::cerr << "[RenderEngine] ERROR: Failed to decode image file: " << path << std::endl;
        if (bitmap) delete bitmap;
        m_imageCache[path] = nullptr;
        return nullptr;
    }

    // Force immediate decompression of JPEG bytes into RAM buffer during preload
    Gdiplus::Color probe;
    bitmap->GetPixel(0, 0, &probe);

    std::cout << "[RenderEngine] Loaded & pre-decoded image '" << path << "' ("
              << bitmap->GetWidth() << "x" << bitmap->GetHeight() << ") from "
              << WideToUtf8(foundPath) << std::endl;

    m_imageCache[path] = bitmap;
    return bitmap;
}

void RenderEngine::DrawQuadImage(const Quad& quad, Gdiplus::Bitmap* bitmap, const std::string& imagePath) {
    if (!bitmap || !m_memDC) return;

    RECT bbox = quad.BoundingBox();
    int dstW = bbox.right - bbox.left;
    int dstH = bbox.bottom - bbox.top;
    if (dstW <= 0 || dstH <= 0) return;

    std::string cacheKey = imagePath.empty() ? std::to_string(reinterpret_cast<uintptr_t>(bitmap)) : imagePath;
    cacheKey += "#" + MakeQuadKey(quad);

    auto it = m_quadImageCache.find(cacheKey);
    if (it != m_quadImageCache.end()) {
        const auto& cached = it->second;
        if (quad.IsAxisAlignedRect()) {
            BitBlt(m_memDC, cached.bbox.left, cached.bbox.top, cached.width, cached.height,
                   cached.hdc, 0, 0, SRCCOPY);
        } else {
            POINT points[4];
            for (int i = 0; i < 4; ++i) {
                points[i].x = quad.corners[i].x;
                points[i].y = quad.corners[i].y;
            }
            HRGN rgn = CreatePolygonRgn(points, 4, WINDING);
            SelectClipRgn(m_memDC, rgn);
            BitBlt(m_memDC, cached.bbox.left, cached.bbox.top, cached.width, cached.height,
                   cached.hdc, 0, 0, SRCCOPY);
            SelectClipRgn(m_memDC, NULL);
            DeleteObject(rgn);
        }
        return;
    }

    // First time rendering: Pre-render and cache into an offscreen GDI bitmap
    UINT imgW = bitmap->GetWidth();
    UINT imgH = bitmap->GetHeight();
    if (imgW == 0 || imgH == 0) return;

    double targetAspect = static_cast<double>(dstW) / static_cast<double>(dstH);
    double imgAspect = static_cast<double>(imgW) / static_cast<double>(imgH);

    double srcX = 0.0;
    double srcY = 0.0;
    double srcW = static_cast<double>(imgW);
    double srcH = static_cast<double>(imgH);

    if (imgAspect > targetAspect) {
        srcW = imgH * targetAspect;
        srcX = (imgW - srcW) / 2.0;
    } else {
        srcH = imgW / targetAspect;
        srcY = (imgH - srcH) / 2.0;
    }

    HDC cacheDC = CreateCompatibleDC(m_memDC);
    HBITMAP cacheBitmap = CreateCompatibleBitmap(m_memDC, dstW, dstH);
    HBITMAP cacheOldBitmap = (HBITMAP)SelectObject(cacheDC, cacheBitmap);

    {
        Gdiplus::Graphics graphics(cacheDC);
        graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBilinear);
        graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);

        // Local quad points relative to bbox
        Gdiplus::PointF localPoints[4] = {
            Gdiplus::PointF(static_cast<Gdiplus::REAL>(quad.corners[0].x - bbox.left), static_cast<Gdiplus::REAL>(quad.corners[0].y - bbox.top)),
            Gdiplus::PointF(static_cast<Gdiplus::REAL>(quad.corners[1].x - bbox.left), static_cast<Gdiplus::REAL>(quad.corners[1].y - bbox.top)),
            Gdiplus::PointF(static_cast<Gdiplus::REAL>(quad.corners[2].x - bbox.left), static_cast<Gdiplus::REAL>(quad.corners[2].y - bbox.top)),
            Gdiplus::PointF(static_cast<Gdiplus::REAL>(quad.corners[3].x - bbox.left), static_cast<Gdiplus::REAL>(quad.corners[3].y - bbox.top))
        };

        Gdiplus::GraphicsPath clipPath;
        clipPath.AddPolygon(localPoints, 4);
        graphics.SetClip(&clipPath);

        Gdiplus::RectF dstRect(0.0f, 0.0f, static_cast<Gdiplus::REAL>(dstW), static_cast<Gdiplus::REAL>(dstH));
        graphics.DrawImage(bitmap, dstRect,
                           static_cast<Gdiplus::REAL>(srcX), static_cast<Gdiplus::REAL>(srcY),
                           static_cast<Gdiplus::REAL>(srcW), static_cast<Gdiplus::REAL>(srcH),
                           Gdiplus::UnitPixel);
    }

    CachedQuadImage entry;
    entry.hdc = cacheDC;
    entry.hBitmap = cacheBitmap;
    entry.hOldBitmap = cacheOldBitmap;
    entry.width = dstW;
    entry.height = dstH;
    entry.bbox = bbox;
    m_quadImageCache[cacheKey] = entry;

    if (quad.IsAxisAlignedRect()) {
        BitBlt(m_memDC, bbox.left, bbox.top, dstW, dstH, cacheDC, 0, 0, SRCCOPY);
    } else {
        POINT points[4];
        for (int i = 0; i < 4; ++i) {
            points[i].x = quad.corners[i].x;
            points[i].y = quad.corners[i].y;
        }
        HRGN rgn = CreatePolygonRgn(points, 4, WINDING);
        SelectClipRgn(m_memDC, rgn);
        BitBlt(m_memDC, bbox.left, bbox.top, dstW, dstH, cacheDC, 0, 0, SRCCOPY);
        SelectClipRgn(m_memDC, NULL);
        DeleteObject(rgn);
    }
}

void RenderEngine::DrawQuadText(const Quad& quad, const std::string& text,
                                const std::string& fontFace, int fontSize,
                                COLORREF color) {
    if (text.empty() || !m_memDC) return;

    RECT bbox = quad.BoundingBox();
    int dstW = bbox.right - bbox.left;
    int dstH = bbox.bottom - bbox.top;
    if (dstW <= 0 || dstH <= 0) return;

    std::string cacheKey = text + "|" + fontFace + "|" + std::to_string(fontSize) + "|" +
                           std::to_string(color) + "#" + MakeQuadKey(quad);

    auto it = m_quadTextCache.find(cacheKey);
    if (it != m_quadTextCache.end()) {
        const auto& cached = it->second;
        BLENDFUNCTION bf = {};
        bf.BlendOp = AC_SRC_OVER;
        bf.BlendFlags = 0;
        bf.SourceConstantAlpha = 255;
        bf.AlphaFormat = AC_SRC_ALPHA;

        AlphaBlend(m_memDC, cached.bbox.left, cached.bbox.top, cached.width, cached.height,
                   cached.hdc, 0, 0, cached.width, cached.height, bf);
        return;
    }

    // Pre-render antialiased text into 32-bit premultiplied DIB once
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = dstW;
    bmi.bmiHeader.biHeight = -dstH; // Top-down DIB
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* dibBits = nullptr;
    HDC textDC = CreateCompatibleDC(m_memDC);
    HBITMAP textBitmap = CreateDIBSection(textDC, &bmi, DIB_RGB_COLORS, &dibBits, NULL, 0);
    HBITMAP textOldBitmap = (HBITMAP)SelectObject(textDC, textBitmap);

    std::memset(dibBits, 0, static_cast<size_t>(dstW) * static_cast<size_t>(dstH) * 4);

    {
        Gdiplus::Graphics graphics(textDC);
        graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);

        // Clip locally
        Gdiplus::PointF localPoints[4] = {
            Gdiplus::PointF(static_cast<Gdiplus::REAL>(quad.corners[0].x - bbox.left), static_cast<Gdiplus::REAL>(quad.corners[0].y - bbox.top)),
            Gdiplus::PointF(static_cast<Gdiplus::REAL>(quad.corners[1].x - bbox.left), static_cast<Gdiplus::REAL>(quad.corners[1].y - bbox.top)),
            Gdiplus::PointF(static_cast<Gdiplus::REAL>(quad.corners[2].x - bbox.left), static_cast<Gdiplus::REAL>(quad.corners[2].y - bbox.top)),
            Gdiplus::PointF(static_cast<Gdiplus::REAL>(quad.corners[3].x - bbox.left), static_cast<Gdiplus::REAL>(quad.corners[3].y - bbox.top))
        };

        Gdiplus::GraphicsPath clipPath;
        clipPath.AddPolygon(localPoints, 4);
        graphics.SetClip(&clipPath);

        std::wstring wideFontFace = Utf8ToWide(fontFace.empty() ? "Arial" : fontFace);
        std::wstring wideText = Utf8ToWide(text);

        Gdiplus::FontFamily* family = nullptr;

        if (m_fontCollection) {
            int pfcCount = m_fontCollection->GetFamilyCount();
            if (pfcCount > 0) {
                std::vector<Gdiplus::FontFamily> pfcFamilies(pfcCount);
                int numFound = 0;
                m_fontCollection->GetFamilies(pfcCount, pfcFamilies.data(), &numFound);
                for (int i = 0; i < numFound; ++i) {
                    WCHAR famName[LF_FACESIZE] = {0};
                    pfcFamilies[i].GetFamilyName(famName);
                    std::wstring famStr(famName);

                    if (_wcsicmp(famName, wideFontFace.c_str()) == 0 ||
                        (wideFontFace.find(L"Bebas") != std::wstring::npos && famStr.find(L"Bebas") != std::wstring::npos)) {
                        family = pfcFamilies[i].Clone();
                        break;
                    }
                }
            }
        }

        if (!family) {
            Gdiplus::FontFamily* sysFam = new Gdiplus::FontFamily(wideFontFace.c_str());
            if (sysFam && sysFam->IsAvailable()) {
                family = sysFam;
            } else {
                delete sysFam;
            }
        }

        if (!family || !family->IsAvailable()) {
            delete family;
            family = new Gdiplus::FontFamily(L"Arial");
        }

        std::string lowerFace = fontFace;
        for (char& c : lowerFace) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));

        int fontStyle = Gdiplus::FontStyleRegular;
        if (lowerFace.find("bold") != std::string::npos || lowerFace.find("negrita") != std::string::npos) {
            fontStyle = Gdiplus::FontStyleBold;
        }

        if (!family->IsStyleAvailable(fontStyle)) {
            fontStyle = Gdiplus::FontStyleRegular;
        }

        Gdiplus::StringFormat* format = Gdiplus::StringFormat::GenericTypographic()->Clone();
        format->SetAlignment(Gdiplus::StringAlignmentCenter);
        format->SetLineAlignment(Gdiplus::StringAlignmentCenter);
        format->SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap | Gdiplus::StringFormatFlagsMeasureTrailingSpaces);

        Gdiplus::REAL effectiveFontSize = static_cast<Gdiplus::REAL>(fontSize);
        if (effectiveFontSize <= 0.0f) {
            Gdiplus::REAL minPt = 8.0f;
            Gdiplus::REAL maxPt = 350.0f;
            Gdiplus::REAL bestPt = 32.0f;

            for (int iter = 0; iter < 18; ++iter) {
                Gdiplus::REAL midPt = (minPt + maxPt) / 2.0f;
                Gdiplus::Font testFont(family, midPt, fontStyle, Gdiplus::UnitPoint);
                Gdiplus::RectF bounds;
                graphics.MeasureString(wideText.c_str(), -1, &testFont, Gdiplus::PointF(0, 0), format, &bounds);
                if (bounds.Width <= static_cast<Gdiplus::REAL>(dstW)) {
                    bestPt = midPt;
                    minPt = midPt;
                } else {
                    maxPt = midPt;
                }
            }
            effectiveFontSize = bestPt;
        }

        Gdiplus::Font font(family, effectiveFontSize, fontStyle, Gdiplus::UnitPoint);
        delete family;

        Gdiplus::RectF layoutRect(0.0f, 0.0f, static_cast<Gdiplus::REAL>(dstW), static_cast<Gdiplus::REAL>(dstH));
        Gdiplus::SolidBrush brush(Gdiplus::Color(255, GetRValue(color), GetGValue(color), GetBValue(color)));

        graphics.DrawString(wideText.c_str(), -1, &font, layoutRect, format, &brush);
        delete format;
    }

    // Convert drawn antialiased pixels into premultiplied ARGB for fast AlphaBlend
    uint32_t* px = static_cast<uint32_t*>(dibBits);
    size_t totalPixels = static_cast<size_t>(dstW) * static_cast<size_t>(dstH);
    for (size_t i = 0; i < totalPixels; ++i) {
        uint32_t val = px[i];
        uint32_t a = (val >> 24) & 0xFF;
        if (a > 0 && a < 255) {
            uint32_t r = (((val >> 16) & 0xFF) * a) / 255;
            uint32_t g = (((val >> 8) & 0xFF) * a) / 255;
            uint32_t b = ((val & 0xFF) * a) / 255;
            px[i] = (a << 24) | (r << 16) | (g << 8) | b;
        }
    }

    CachedQuadText entry;
    entry.hdc = textDC;
    entry.hBitmap = textBitmap;
    entry.hOldBitmap = textOldBitmap;
    entry.width = dstW;
    entry.height = dstH;
    entry.bbox = bbox;
    m_quadTextCache[cacheKey] = entry;

    BLENDFUNCTION bf = {};
    bf.BlendOp = AC_SRC_OVER;
    bf.BlendFlags = 0;
    bf.SourceConstantAlpha = 255;
    bf.AlphaFormat = AC_SRC_ALPHA;

    AlphaBlend(m_memDC, bbox.left, bbox.top, dstW, dstH,
               textDC, 0, 0, dstW, dstH, bf);
}

// ---------------------------------------------------------------------------
// Drawing primitives (used by the calibration overlay)
// ---------------------------------------------------------------------------

HPEN RenderEngine::GetPen(COLORREF color, int thickness) {
    if (thickness < 1) thickness = 1;

    for (const auto& cached : m_pens) {
        if (cached.color == color && cached.thickness == thickness) return cached.pen;
    }

    HPEN pen = CreatePen(PS_SOLID, thickness, color);
    if (!pen) return (HPEN)GetStockObject(WHITE_PEN);

    CachedPen entry = { color, thickness, pen };
    m_pens.push_back(entry);
    return pen;
}

HBRUSH RenderEngine::GetSolidBrush(COLORREF color) {
    auto it = m_brushCache.find(color);
    if (it != m_brushCache.end()) return it->second;

    HBRUSH brush = CreateSolidBrush(color);
    if (!brush) return (HBRUSH)GetStockObject(WHITE_BRUSH);

    m_brushCache[color] = brush;
    return brush;
}

void RenderEngine::FillQuadWithBrush(const Quad& quad, HBRUSH brush) {
    POINT points[4];
    for (int i = 0; i < 4; ++i) {
        points[i].x = quad.corners[i].x;
        points[i].y = quad.corners[i].y;
    }

    HGDIOBJ oldBrush = SelectObject(m_memDC, brush);
    HGDIOBJ oldPen   = SelectObject(m_memDC, GetStockObject(NULL_PEN));
    Polygon(m_memDC, points, 4);
    SelectObject(m_memDC, oldPen);
    SelectObject(m_memDC, oldBrush);
}

void RenderEngine::FillQuad(const Quad& quad, COLORREF color) {
    if (!m_memDC) return;

    HBRUSH brush = (color == RGB(255, 255, 255) && m_whiteBrush) ? m_whiteBrush : GetSolidBrush(color);

    if (quad.IsAxisAlignedRect()) {
        RECT rect = quad.BoundingBox();
        FillRect(m_memDC, &rect, brush);
        return;
    }

    FillQuadWithBrush(quad, brush);
}

void RenderEngine::FillQuadHalftone(const Quad& quad, COLORREF color) {
    if (!m_memDC || !m_halftoneBrush) return;

    COLORREF oldText = SetTextColor(m_memDC, color);
    COLORREF oldBk   = SetBkColor(m_memDC, RGB(0, 0, 0));

    if (quad.IsAxisAlignedRect()) {
        RECT rect = quad.BoundingBox();
        FillRect(m_memDC, &rect, m_halftoneBrush);
    } else {
        FillQuadWithBrush(quad, m_halftoneBrush);
    }

    SetTextColor(m_memDC, oldText);
    SetBkColor(m_memDC, oldBk);
}

void RenderEngine::DrawQuadOutline(const Quad& quad, COLORREF color, int thickness) {
    if (!m_memDC) return;

    POINT points[5];
    for (int i = 0; i < 4; ++i) {
        points[i].x = quad.corners[i].x;
        points[i].y = quad.corners[i].y;
    }
    points[4] = points[0]; // Close the loop

    HGDIOBJ oldPen = SelectObject(m_memDC, GetPen(color, thickness));
    Polyline(m_memDC, points, 5);
    SelectObject(m_memDC, oldPen);
}

void RenderEngine::DrawHandle(const Point2i& point, int halfSize, COLORREF fill, COLORREF border) {
    if (!m_memDC) return;

    RECT rect = { point.x - halfSize, point.y - halfSize,
                  point.x + halfSize + 1, point.y + halfSize + 1 };

    HBRUSH brush = GetSolidBrush(fill);
    FillRect(m_memDC, &rect, brush);

    HGDIOBJ oldPen   = SelectObject(m_memDC, GetPen(border, 1));
    HGDIOBJ oldBrush = SelectObject(m_memDC, GetStockObject(NULL_BRUSH));
    Rectangle(m_memDC, rect.left, rect.top, rect.right, rect.bottom);
    SelectObject(m_memDC, oldBrush);
    SelectObject(m_memDC, oldPen);
}

void RenderEngine::DrawCross(const Point2i& point, int armLength, COLORREF color) {
    if (!m_memDC) return;

    HGDIOBJ oldPen = SelectObject(m_memDC, GetPen(color, 1));
    MoveToEx(m_memDC, point.x - armLength, point.y, NULL);
    LineTo(m_memDC, point.x + armLength + 1, point.y);
    MoveToEx(m_memDC, point.x, point.y - armLength, NULL);
    LineTo(m_memDC, point.x, point.y + armLength + 1);
    SelectObject(m_memDC, oldPen);
}

void RenderEngine::EnsureScratchBuffer(int minWidth, int minHeight) {
    if (m_scratchDC && m_scratchBitmap && minWidth <= m_scratchWidth && minHeight <= m_scratchHeight) {
        return;
    }

    if (m_scratchDC) {
        if (m_scratchOldBitmap) SelectObject(m_scratchDC, m_scratchOldBitmap);
        if (m_scratchBitmap) DeleteObject(m_scratchBitmap);
        DeleteDC(m_scratchDC);
        m_scratchDC = NULL;
        m_scratchBitmap = NULL;
        m_scratchBits = NULL;
        m_scratchWidth = 0;
        m_scratchHeight = 0;
    }

    m_scratchWidth = (minWidth > m_scratchWidth) ? minWidth : m_scratchWidth;
    m_scratchHeight = (minHeight > m_scratchHeight) ? minHeight : m_scratchHeight;
    if (m_scratchWidth < 512) m_scratchWidth = 512;
    if (m_scratchHeight < 512) m_scratchHeight = 512;

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = m_scratchWidth;
    bmi.bmiHeader.biHeight = -m_scratchHeight; // Top-down DIB
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    m_scratchDC = CreateCompatibleDC(m_memDC ? m_memDC : m_hdc);
    m_scratchBitmap = CreateDIBSection(m_scratchDC, &bmi, DIB_RGB_COLORS, &m_scratchBits, NULL, 0);
    if (m_scratchDC && m_scratchBitmap) {
        m_scratchOldBitmap = (HBITMAP)SelectObject(m_scratchDC, m_scratchBitmap);
    }
}

void RenderEngine::EnsureBlackBitmap() {
    if (m_blackDC && m_blackBitmap) return;

    HDC refDC = m_memDC ? m_memDC : m_hdc;
    if (!refDC) return;

    m_blackDC = CreateCompatibleDC(refDC);
    if (!m_blackDC) return;

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = 16;
    bmi.bmiHeader.biHeight = 16;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    m_blackBitmap = CreateDIBSection(m_blackDC, &bmi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (m_blackBitmap && bits) {
        std::memset(bits, 0, 16 * 16 * 4);
    }
    if (m_blackDC && m_blackBitmap) {
        m_blackOldBitmap = (HBITMAP)SelectObject(m_blackDC, m_blackBitmap);
    }
}

void RenderEngine::DrawHudText(int x, int y, const std::wstring& text, COLORREF color,
                              BYTE bgAlpha, BYTE textAlpha) {
    if (!m_memDC || text.empty()) return;

    HGDIOBJ oldFont = m_hudFont ? SelectObject(m_memDC, m_hudFont) : NULL;

    const UINT format = DT_LEFT | DT_TOP | DT_NOPREFIX | DT_EXPANDTABS;

    RECT measured = { 0, 0, 0, 0 };
    DrawTextW(m_memDC, text.c_str(), static_cast<int>(text.size()), &measured,
              format | DT_CALCRECT);

    if (oldFont) SelectObject(m_memDC, oldFont);

    int textW = measured.right - measured.left;
    int textH = measured.bottom - measured.top;
    if (textW <= 0 || textH <= 0) return;

    int totalW = textW + kHudTextPadding * 2;
    int totalH = textH + kHudTextPadding * 2;

    EnsureScratchBuffer(totalW, totalH);
    if (!m_scratchDC || !m_scratchBits) return;

    // Clear the active (totalW x totalH) rectangle to 0 in scratch buffer
    uint32_t* px = static_cast<uint32_t*>(m_scratchBits);
    for (int row = 0; row < totalH; ++row) {
        std::memset(&px[row * m_scratchWidth], 0, totalW * sizeof(uint32_t));
    }

    // Select font in scratch DC
    HGDIOBJ oldScratchFont = m_hudFont ? SelectObject(m_scratchDC, m_hudFont) : NULL;
    int oldMode = SetBkMode(m_scratchDC, TRANSPARENT);
    COLORREF oldColor = SetTextColor(m_scratchDC, RGB(255, 255, 255));

    RECT textRect = { kHudTextPadding, kHudTextPadding,
                      totalW - kHudTextPadding, totalH - kHudTextPadding };
    DrawTextW(m_scratchDC, text.c_str(), static_cast<int>(text.size()), &textRect, format);

    SetTextColor(m_scratchDC, oldColor);
    SetBkMode(m_scratchDC, oldMode);
    if (oldScratchFont) SelectObject(m_scratchDC, oldScratchFont);

    const int targetR = GetRValue(color);
    const int targetG = GetGValue(color);
    const int targetB = GetBValue(color);

    for (int row = 0; row < totalH; ++row) {
        uint32_t* rowPx = &px[row * m_scratchWidth];
        for (int col = 0; col < totalW; ++col) {
            uint32_t val = rowPx[col];
            int glyph = val & 0xFF;

            if (glyph == 0) {
                rowPx[col] = (static_cast<uint32_t>(bgAlpha) << 24);
            } else {
                int a = bgAlpha + ((textAlpha - bgAlpha) * glyph) / 255;
                int r = (targetR * glyph) / 255;
                int g = (targetG * glyph) / 255;
                int b = (targetB * glyph) / 255;
                int r_pre = (r * a) / 255;
                int g_pre = (g * a) / 255;
                int b_pre = (b * a) / 255;
                rowPx[col] = (static_cast<uint32_t>(a) << 24) |
                             (static_cast<uint32_t>(r_pre) << 16) |
                             (static_cast<uint32_t>(g_pre) << 8) |
                             static_cast<uint32_t>(b_pre);
            }
        }
    }

    int srcX = 0;
    int srcY = 0;
    int dstX = x;
    int dstY = y;
    int drawW = totalW;
    int drawH = totalH;

    if (dstX < 0) {
        srcX -= dstX;
        drawW += dstX;
        dstX = 0;
    }
    if (dstY < 0) {
        srcY -= dstY;
        drawH += dstY;
        dstY = 0;
    }
    if (dstX + drawW > m_width) {
        drawW = m_width - dstX;
    }
    if (dstY + drawH > m_height) {
        drawH = m_height - dstY;
    }
    if (drawW <= 0 || drawH <= 0) return;

    BLENDFUNCTION bf = {};
    bf.BlendOp = AC_SRC_OVER;
    bf.BlendFlags = 0;
    bf.SourceConstantAlpha = 255;
    bf.AlphaFormat = AC_SRC_ALPHA;

    AlphaBlend(m_memDC, dstX, dstY, drawW, drawH,
               m_scratchDC, srcX, srcY, drawW, drawH, bf);
}
