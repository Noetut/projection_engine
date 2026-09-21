#include "AnimationParser.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <windows.h>

#include "util/StringUtil.h"

namespace {

std::string Trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

std::string ToUpper(std::string str) {
    std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });
    return str;
}

bool IsDigitString(const std::string& str) {
    if (str.empty()) return false;
    for (char c : str) {
        if (!std::isdigit(static_cast<unsigned char>(c))) return false;
    }
    return true;
}

bool NextTokenOrQuoted(std::istream& is, std::string& outToken) {
    outToken.clear();
    char ch = 0;
    while (is.get(ch) && (ch == ' ' || ch == '\t')) {}
    if (!is) return false;

    if (ch == '"' || ch == '\'') {
        char quote = ch;
        while (is.get(ch)) {
            if (ch == quote) break;
            outToken += ch;
        }
        return true;
    } else {
        outToken += ch;
        while (is.get(ch) && ch != ' ' && ch != '\t' && ch != '\r' && ch != '\n') {
            outToken += ch;
        }
        return true;
    }
}

// Strip inline comments starting with '#' or '//'
std::string StripComments(const std::string& line) {
    size_t hashPos = line.find('#');
    size_t slashPos = line.find("//");
    size_t pos = std::min(hashPos, slashPos);
    if (pos != std::string::npos) {
        return line.substr(0, pos);
    }
    return line;
}

// Try parsing duration string like "300ms", "0.5s", "300", "@250ms"
bool TryParseDuration(std::string token, double& outSeconds) {
    token = Trim(token);
    if (token.empty()) return false;
    if (token[0] == '@') token = token.substr(1);

    std::string upper = ToUpper(token);
    if (upper.size() > 2 && upper.substr(upper.size() - 2) == "MS") {
        try {
            double ms = std::stod(token.substr(0, token.size() - 2));
            outSeconds = ms / 1000.0;
            return true;
        } catch (...) {
            return false;
        }
    }

    if (upper.size() > 1 && upper.back() == 'S') {
        try {
            double s = std::stod(token.substr(0, token.size() - 1));
            outSeconds = s;
            return true;
        } catch (...) {
            return false;
        }
    }

    // Pure number defaults to milliseconds
    try {
        size_t idx = 0;
        double val = std::stod(token, &idx);
        if (idx == token.size()) {
            outSeconds = val / 1000.0;
            return true;
        }
    } catch (...) {
    }

    return false;
}

// Try parsing wait/delay seconds from token like "1", "1s", "0.5", "500ms", "2.5s"
bool TryParseWaitSeconds(std::string token, double& outSeconds) {
    token = Trim(token);
    if (token.empty()) return false;
    if (token[0] == '@') token = token.substr(1);

    std::string upper = ToUpper(token);
    if (upper.size() > 2 && upper.substr(upper.size() - 2) == "MS") {
        try {
            double ms = std::stod(token.substr(0, token.size() - 2));
            outSeconds = ms / 1000.0;
            return true;
        } catch (...) {
            return false;
        }
    }

    if (upper.size() > 1 && upper.back() == 'S') {
        try {
            double s = std::stod(token.substr(0, token.size() - 1));
            outSeconds = s;
            return true;
        } catch (...) {
            return false;
        }
    }

    // Pure numbers in WAIT / DELAY context are seconds (e.g. "1" -> 1.0s, "0.5" -> 0.5s, "2" -> 2.0s)
    try {
        size_t idx = 0;
        double val = std::stod(token, &idx);
        if (idx == token.size()) {
            outSeconds = val;
            return true;
        }
    } catch (...) {
    }

    return false;
}

bool TryParseColor(const std::string& token, COLORREF& outColor) {
    if (token.empty()) return false;
    std::string upper = ToUpper(token);

    if (upper == "RED" || upper == "ROJO") { outColor = RGB(255, 0, 0); return true; }
    if (upper == "GREEN" || upper == "VERDE") { outColor = RGB(0, 255, 0); return true; }
    if (upper == "BLUE" || upper == "AZUL") { outColor = RGB(0, 80, 255); return true; }
    if (upper == "CYAN" || upper == "AQUA" || upper == "CELESTE") { outColor = RGB(0, 255, 255); return true; }
    if (upper == "MAGENTA" || upper == "FUCHSIA") { outColor = RGB(255, 0, 255); return true; }
    if (upper == "YELLOW" || upper == "AMARILLO") { outColor = RGB(255, 230, 0); return true; }
    if (upper == "ORANGE" || upper == "NARANJA") { outColor = RGB(255, 120, 0); return true; }
    if (upper == "PURPLE" || upper == "MORADO" || upper == "VIOLETA" || upper == "VIOLET") { outColor = RGB(160, 32, 240); return true; }
    if (upper == "PINK" || upper == "ROSA") { outColor = RGB(255, 105, 180); return true; }
    if (upper == "GOLD" || upper == "DORADO") { outColor = RGB(255, 215, 0); return true; }
    if (upper == "LIME" || upper == "LIMA") { outColor = RGB(50, 255, 50); return true; }
    if (upper == "TURQUOISE" || upper == "TURQUESA") { outColor = RGB(64, 224, 208); return true; }
    if (upper == "CORAL") { outColor = RGB(255, 127, 80); return true; }
    if (upper == "CRIMSON") { outColor = RGB(220, 20, 60); return true; }
    if (upper == "WHITE" || upper == "BLANCO") { outColor = RGB(255, 255, 255); return true; }
    if (upper == "BLACK" || upper == "NEGRO") { outColor = RGB(0, 0, 0); return true; }
    if (upper == "NEON_GREEN" || upper == "NEONGREEN") { outColor = RGB(57, 255, 20); return true; }
    if (upper == "NEON_BLUE" || upper == "NEONBLUE") { outColor = RGB(31, 81, 255); return true; }
    if (upper == "NEON_PINK" || upper == "NEONPINK") { outColor = RGB(255, 16, 240); return true; }

    // Hex #RRGGBB or #RGB
    if (upper[0] == '#' && (upper.size() == 7 || upper.size() == 4)) {
        try {
            if (upper.size() == 7) {
                int r = std::stoi(upper.substr(1, 2), nullptr, 16);
                int g = std::stoi(upper.substr(3, 2), nullptr, 16);
                int b = std::stoi(upper.substr(5, 2), nullptr, 16);
                outColor = RGB(r, g, b);
                return true;
            } else {
                int r = std::stoi(upper.substr(1, 1) + upper.substr(1, 1), nullptr, 16);
                int g = std::stoi(upper.substr(2, 1) + upper.substr(2, 1), nullptr, 16);
                int b = std::stoi(upper.substr(3, 1) + upper.substr(3, 1), nullptr, 16);
                outColor = RGB(r, g, b);
                return true;
            }
        } catch (...) {}
    }

    return false;
}

bool FileExists(const std::wstring& path) {
    DWORD attr = GetFileAttributesW(path.c_str());
    return (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY));
}

std::wstring GetExecutableDir() {
    wchar_t buffer[MAX_PATH];
    DWORD len = GetModuleFileNameW(NULL, buffer, MAX_PATH);
    if (len == 0 || len == MAX_PATH) return L".";
    for (int i = static_cast<int>(len) - 1; i >= 0; --i) {
        if (buffer[i] == L'\\' || buffer[i] == L'/') {
            buffer[i] = L'\0';
            break;
        }
    }
    return buffer;
}

} // namespace

std::string AnimationParser::ResolvePath(const std::string& inputPath) {
    if (inputPath.empty()) {
        const char* defaults[] = {
            "animations/area_ciclogenica.txt",
            "../animations/area_ciclogenica.txt",
            "../../animations/area_ciclogenica.txt",
            "animations/strobe_show.txt",
            "animations/sequential_wave.txt",
            "../animations/strobe_show.txt",
            "../animations/sequential_wave.txt",
            "../../animations/strobe_show.txt",
            "../../animations/sequential_wave.txt"
        };
        for (const char* candidate : defaults) {
            std::wstring wide = AnsiToWide(candidate);
            if (FileExists(wide)) return candidate;
        }

        std::wstring exeDir = GetExecutableDir();
        std::wstring cand0 = exeDir + L"\\animations\\area_ciclogenica.txt";
        if (FileExists(cand0)) return WideToUtf8(cand0);
        std::wstring cand0b = exeDir + L"\\..\\animations\\area_ciclogenica.txt";
        if (FileExists(cand0b)) return WideToUtf8(cand0b);
        std::wstring cand1 = exeDir + L"\\animations\\strobe_show.txt";
        if (FileExists(cand1)) return WideToUtf8(cand1);
        std::wstring cand2 = exeDir + L"\\animations\\sequential_wave.txt";
        if (FileExists(cand2)) return WideToUtf8(cand2);
        std::wstring cand3 = exeDir + L"\\..\\animations\\strobe_show.txt";
        if (FileExists(cand3)) return WideToUtf8(cand3);

        return "animations/area_ciclogenica.txt";
    }

    std::wstring direct = AnsiToWide(inputPath);
    if (FileExists(direct)) return inputPath;

    // Check with animations/ prefix
    std::string animPrefix = "animations/" + inputPath;
    if (FileExists(AnsiToWide(animPrefix))) return animPrefix;

    std::string upAnimPrefix = "../animations/" + inputPath;
    if (FileExists(AnsiToWide(upAnimPrefix))) return upAnimPrefix;

    std::wstring exeDir = GetExecutableDir();
    std::wstring candExe = exeDir + L"\\" + direct;
    if (FileExists(candExe)) return WideToUtf8(candExe);

    std::wstring candExeAnim = exeDir + L"\\animations\\" + direct;
    if (FileExists(candExeAnim)) return WideToUtf8(candExeAnim);

    return inputPath;
}

bool AnimationParser::LoadFromFile(const std::string& path, AnimationSequence& outSequence, std::string& outError) {
    std::wstring widePath = AnsiToWide(path);
    HANDLE hFile = CreateFileW(widePath.c_str(), GENERIC_READ, FILE_SHARE_READ,
                               NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        outError = "Cannot open file: " + path;
        return false;
    }

    DWORD fileSize = GetFileSize(hFile, NULL);
    if (fileSize == INVALID_FILE_SIZE) {
        CloseHandle(hFile);
        outError = "Failed to query size for: " + path;
        return false;
    }

    std::string buffer(fileSize, '\0');
    DWORD bytesRead = 0;
    if (!ReadFile(hFile, &buffer[0], fileSize, &bytesRead, NULL)) {
        CloseHandle(hFile);
        outError = "Failed to read file: " + path;
        return false;
    }
    CloseHandle(hFile);
    buffer.resize(bytesRead);

    return ParseString(buffer, outSequence, outError);
}

bool AnimationParser::ParseString(const std::string& content, AnimationSequence& outSequence, std::string& outError) {
    outSequence.frames.clear();
    outSequence.name = "Untitled";
    outSequence.loop = true;
    outSequence.defaultStep = 0.3;

    std::istringstream stream(content);
    std::string rawLine;
    int lineNumber = 0;

    while (std::getline(stream, rawLine)) {
        ++lineNumber;
        std::string line = Trim(StripComments(rawLine));
        if (line.empty()) continue;

        // Check for headers (key: value or key = value)
        size_t colonPos = line.find(':');
        size_t equalPos = line.find('=');
        size_t sepPos = (colonPos != std::string::npos) ? colonPos : equalPos;

        if (sepPos != std::string::npos) {
            std::string key = ToUpper(Trim(line.substr(0, sepPos)));
            std::string val = Trim(line.substr(sepPos + 1));

            if (key == "LOOP") {
                std::string valUpper = ToUpper(val);
                outSequence.loop = (valUpper == "TRUE" || valUpper == "1" || valUpper == "YES");
                continue;
            } else if (key == "DEFAULT_STEP" || key == "STEP" || key == "DEFAULT_DURATION") {
                double stepSec = 0.3;
                if (TryParseDuration(val, stepSec)) {
                    outSequence.defaultStep = stepSec;
                }
                continue;
            } else if (key == "NAME") {
                outSequence.name = val;
                continue;
            } else if (key == "SEGMENT" || key == "CUE") {
                if (!outSequence.frames.empty()) {
                    outSequence.frames.back().waitForClick = true;
                    outSequence.frames.back().segmentName = val;
                }
                continue;
            } else if (key == "RESET_IMAGES" || key == "CLEAR_IMAGES" || key == "ALL_WHITE") {
                std::string valUpper = ToUpper(val);
                outSequence.resetImages = (valUpper == "TRUE" || valUpper == "1" || valUpper == "YES");
                continue;
            } else if (key == "PRELOAD" || key == "PRELOAD_IMAGES" || key == "PRELOAD_IMAGE") {
                std::istringstream iss(val);
                std::string token;
                while (iss >> token) {
                    while (!token.empty() && (token.back() == ',' || token.back() == ';')) token.pop_back();
                    if (!token.empty()) {
                        outSequence.preloadImages.push_back(token);
                    }
                }
                continue;
            } else if (key == "PRELOAD_VIDEOS" || key == "PRELOAD_VIDEO") {
                std::istringstream iss(val);
                std::string token;
                while (iss >> token) {
                    while (!token.empty() && (token.back() == ',' || token.back() == ';')) token.pop_back();
                    if (!token.empty()) {
                        outSequence.preloadVideos.push_back(token);
                    }
                }
                continue;
            }
        }

        // Frame line format: [duration] ACTION1 [& ACTION2 ...]
        // Or standalone wait: WAIT 1, WAIT 1.5s, 0.5s, etc.
        std::istringstream lineStream(line);
        std::string firstToken;
        lineStream >> firstToken;

        double frameDuration = outSequence.defaultStep;
        bool hasDuration = false;

        std::string firstUpper = ToUpper(firstToken);
        if (firstUpper == "WAIT" || firstUpper == "DELAY" || firstUpper == "SLEEP" || firstUpper == "PAUSE") {
            std::string waitArg;
            std::streampos posBefore = lineStream.tellg();
            if (lineStream >> waitArg) {
                double waitSec = 0.0;
                if (TryParseWaitSeconds(waitArg, waitSec)) {
                    frameDuration = waitSec;
                    hasDuration = true;
                } else {
                    // Not a duration argument, rewind
                    lineStream.clear();
                    lineStream.seekg(posBefore);
                }
            }
        } else {
            double parsedDuration = 0.0;
            if (TryParseDuration(firstToken, parsedDuration)) {
                frameDuration = parsedDuration;
                hasDuration = true;
            }
        }

        // Collect remaining tokens on this line
        std::string commandPart;
        if (hasDuration) {
            std::getline(lineStream, commandPart);
        } else {
            // First token was part of the command
            std::string rest;
            std::getline(lineStream, rest);
            commandPart = firstToken + " " + rest;
        }
        commandPart = Trim(commandPart);
        if (commandPart.empty()) {
            // Just a delay / wait frame (e.g. "WAIT 1" or "1.0s")
            // If the previous frame didn't specify an explicit duration and has actions,
            // update its duration directly so the delay applies to that state without extra latency.
            if (!outSequence.frames.empty() &&
                !outSequence.frames.back().hasExplicitDuration &&
                !outSequence.frames.back().actions.empty()) {
                outSequence.frames.back().duration = frameDuration;
                outSequence.frames.back().hasExplicitDuration = true;
            } else {
                AnimationFrame waitFrame;
                waitFrame.duration = frameDuration;
                waitFrame.hasExplicitDuration = true;
                outSequence.frames.push_back(waitFrame);
            }
            continue;
        }

        // Split multiple actions separated by '&'
        std::vector<std::string> subCommands;
        {
            std::istringstream cmdStream(commandPart);
            std::string subCmd;
            while (std::getline(cmdStream, subCmd, '&')) {
                subCmd = Trim(subCmd);
                if (!subCmd.empty()) {
                    subCommands.push_back(subCmd);
                }
            }
        }

        AnimationFrame frame;
        frame.duration = frameDuration;
        frame.hasExplicitDuration = hasDuration;

        for (const auto& cmdStr : subCommands) {
            std::istringstream cs(cmdStr);
            std::string verb;
            cs >> verb;
            std::string verbUpper = ToUpper(verb);

            if (verbUpper == "ALL_ON" || verbUpper == "ALLON") {
                AnimationAction action;
                action.type = ActionType::AllOn;
                std::string colToken;
                if (cs >> colToken) {
                    COLORREF c;
                    if (TryParseColor(colToken, c)) {
                        action.color = c;
                    }
                }
                frame.actions.push_back(action);
            } else if (verbUpper == "ALL_OFF" || verbUpper == "ALLOFF" || verbUpper == "CLEAR") {
                AnimationAction action;
                action.type = ActionType::AllOff;
                frame.actions.push_back(action);
            } else if (verbUpper == "ON") {
                std::vector<std::string> tokens;
                std::string t;
                while (cs >> t) {
                    tokens.push_back(t);
                }
                COLORREF color = RGB(255, 255, 255);
                std::vector<std::string> targetTokens;
                for (const auto& tok : tokens) {
                    COLORREF parsedCol;
                    if (TryParseColor(tok, parsedCol)) {
                        color = parsedCol;
                    } else {
                        targetTokens.push_back(tok);
                    }
                }
                if (targetTokens.empty() || (targetTokens.size() == 1 && ToUpper(targetTokens[0]) == "ALL")) {
                    AnimationAction action;
                    action.type = ActionType::AllOn;
                    action.color = color;
                    frame.actions.push_back(action);
                } else {
                    for (const auto& tgt : targetTokens) {
                        AnimationAction action;
                        action.type = ActionType::TurnOn;
                        action.color = color;
                        if (IsDigitString(tgt)) {
                            action.targetId = std::stoi(tgt);
                            action.targetIndex = action.targetId - 1;
                        } else {
                            action.targetName = tgt;
                        }
                        frame.actions.push_back(action);
                    }
                }
            } else if (verbUpper == "COLOR" || verbUpper == "SET_COLOR" || verbUpper == "SETCOLOR") {
                std::vector<std::string> tokens;
                std::string t;
                while (cs >> t) {
                    tokens.push_back(t);
                }
                COLORREF color = RGB(255, 255, 255);
                std::vector<std::string> targetTokens;
                for (const auto& tok : tokens) {
                    COLORREF parsedCol;
                    if (TryParseColor(tok, parsedCol)) {
                        color = parsedCol;
                    } else {
                        targetTokens.push_back(tok);
                    }
                }
                if (targetTokens.empty() || (targetTokens.size() == 1 && ToUpper(targetTokens[0]) == "ALL")) {
                    AnimationAction action;
                    action.type = ActionType::SetColor;
                    action.targetId = -2; // ALL
                    action.color = color;
                    frame.actions.push_back(action);
                } else {
                    for (const auto& tgt : targetTokens) {
                        AnimationAction action;
                        action.type = ActionType::SetColor;
                        action.color = color;
                        if (IsDigitString(tgt)) {
                            action.targetId = std::stoi(tgt);
                            action.targetIndex = action.targetId - 1;
                        } else {
                            action.targetName = tgt;
                        }
                        frame.actions.push_back(action);
                    }
                }
            } else if (verbUpper == "OFF") {
                std::string arg;
                cs >> arg;
                std::string argUpper = ToUpper(arg);
                if (argUpper == "ALL") {
                    AnimationAction action;
                    action.type = ActionType::AllOff;
                    frame.actions.push_back(action);
                } else if (IsDigitString(arg)) {
                    AnimationAction action;
                    action.type = ActionType::TurnOff;
                    action.targetId = std::stoi(arg);
                    action.targetIndex = action.targetId - 1;
                    frame.actions.push_back(action);
                } else if (!arg.empty()) {
                    AnimationAction action;
                    action.type = ActionType::TurnOff;
                    action.targetName = arg;
                    frame.actions.push_back(action);
                }
            } else if (verbUpper == "TOGGLE") {
                std::string arg;
                cs >> arg;
                if (IsDigitString(arg)) {
                    AnimationAction action;
                    action.type = ActionType::Toggle;
                    action.targetId = std::stoi(arg);
                    action.targetIndex = action.targetId - 1;
                    frame.actions.push_back(action);
                } else if (!arg.empty()) {
                    AnimationAction action;
                    action.type = ActionType::Toggle;
                    action.targetName = arg;
                    frame.actions.push_back(action);
                }
            } else if (verbUpper == "MASK") {
                std::string maskStr;
                cs >> maskStr;
                AnimationAction action;
                action.type = ActionType::SetMask;
                for (char c : maskStr) {
                    action.mask.push_back(c == '1');
                }
                frame.actions.push_back(action);
            } else if (verbUpper == "IMAGE" || verbUpper == "IMG" || verbUpper == "LOAD_IMAGE") {
                std::string target;
                std::string imgName;
                cs >> target >> imgName;
                if (!target.empty()) {
                    AnimationAction action;
                    action.type = ActionType::SetImage;
                    if (IsDigitString(target)) {
                        action.targetId = std::stoi(target);
                        action.targetIndex = action.targetId - 1;
                    } else {
                        action.targetName = target;
                    }
                    action.imagePath = imgName;
                    frame.actions.push_back(action);
                }
            } else if (verbUpper == "CLEAR_IMAGE" || verbUpper == "CLEARIMAGE") {
                std::string target;
                cs >> target;
                std::string targetUpper = ToUpper(target);
                if (targetUpper == "ALL" || targetUpper == "*" || targetUpper.empty()) {
                    AnimationAction action;
                    action.type = ActionType::ClearAllImages;
                    frame.actions.push_back(action);
                } else {
                    AnimationAction action;
                    action.type = ActionType::ClearImage;
                    if (IsDigitString(target)) {
                        action.targetId = std::stoi(target);
                        action.targetIndex = action.targetId - 1;
                    } else {
                        action.targetName = target;
                    }
                    frame.actions.push_back(action);
                }
            } else if (verbUpper == "CLEAR_IMAGES" || verbUpper == "CLEARALLIMAGES" ||
                       verbUpper == "CLEAR_ALL_IMAGES" || verbUpper == "ALL_WHITE" ||
                       verbUpper == "ALLWHITE" || verbUpper == "RESET_IMAGES") {
                AnimationAction actionImg;
                actionImg.type = ActionType::ClearAllImages;
                frame.actions.push_back(actionImg);
                AnimationAction actionTxt;
                actionTxt.type = ActionType::ClearAllTexts;
                frame.actions.push_back(actionTxt);
            } else if (verbUpper == "TEXT" || verbUpper == "SET_TEXT" || verbUpper == "SETTEXT") {
                std::string target, tok2, tok3, tok4;
                if (NextTokenOrQuoted(cs, target)) {
                    AnimationAction action;
                    action.type = ActionType::SetText;
                    if (IsDigitString(target)) {
                        action.targetId = std::stoi(target);
                        action.targetIndex = action.targetId - 1;
                    } else {
                        action.targetName = target;
                    }

                    if (NextTokenOrQuoted(cs, tok2)) {
                        if (NextTokenOrQuoted(cs, tok3)) {
                            if (NextTokenOrQuoted(cs, tok4)) {
                                std::string tok5;
                                if (NextTokenOrQuoted(cs, tok5)) {
                                    // 4 arguments after target: font, size, style, text
                                    action.fontFace = tok2 + " " + tok4;
                                    if (IsDigitString(tok3)) {
                                        action.fontSize = std::stoi(tok3);
                                    }
                                    action.text = tok5;
                                } else {
                                    // 3 arguments after target: font, size, text
                                    action.fontFace = tok2;
                                    if (IsDigitString(tok3)) {
                                        action.fontSize = std::stoi(tok3);
                                    }
                                    action.text = tok4;
                                }
                            } else {
                                // 2 arguments after target:
                                if (IsDigitString(tok2)) {
                                    // size, text
                                    action.fontSize = std::stoi(tok2);
                                    action.fontFace = "Arial";
                                    action.text = tok3;
                                } else if (IsDigitString(tok3)) {
                                    // font, size, text empty
                                    action.fontFace = tok2;
                                    action.fontSize = std::stoi(tok3);
                                } else {
                                    // font, text
                                    action.fontFace = tok2;
                                    action.text = tok3;
                                }
                            }
                        } else {
                            // 1 argument after target: just the text
                            action.text = tok2;
                            action.fontFace = "Arial";
                            action.fontSize = 32;
                        }
                    }
                    frame.actions.push_back(action);
                }
            } else if (verbUpper == "CLEAR_TEXT" || verbUpper == "CLEARTEXT") {
                std::string target;
                cs >> target;
                std::string targetUpper = ToUpper(target);
                if (targetUpper == "ALL" || targetUpper == "*" || targetUpper.empty()) {
                    AnimationAction action;
                    action.type = ActionType::ClearAllTexts;
                    frame.actions.push_back(action);
                } else {
                    AnimationAction action;
                    action.type = ActionType::ClearText;
                    if (IsDigitString(target)) {
                        action.targetId = std::stoi(target);
                        action.targetIndex = action.targetId - 1;
                    } else {
                        action.targetName = target;
                    }
                    frame.actions.push_back(action);
                }
            } else if (verbUpper == "PRELOAD" || verbUpper == "PRELOAD_IMAGE") {
                std::string imgName;
                cs >> imgName;
                if (!imgName.empty()) {
                    AnimationAction action;
                    action.type = ActionType::PreloadImage;
                    action.imagePath = imgName;
                    frame.actions.push_back(action);
                }
            } else if (verbUpper == "BG_VIDEO" || verbUpper == "BGVIDEO" ||
                       verbUpper == "VIDEO_BG" || verbUpper == "VIDEOBG" ||
                       verbUpper == "VIDEO" || verbUpper == "PLAY_VIDEO") {
                std::string videoName;
                if (NextTokenOrQuoted(cs, videoName)) {
                    AnimationAction action;
                    action.type = ActionType::SetBackgroundVideo;
                    action.videoPath = videoName;
                    action.fadeDuration = 1.0;

                    std::string extra;
                    while (cs >> extra) {
                        std::string upperExtra = ToUpper(extra);
                        if (upperExtra.rfind("FADE=", 0) == 0) {
                            double f = 1.0;
                            if (TryParseDuration(extra.substr(5), f)) {
                                action.fadeDuration = f;
                            }
                        } else if (upperExtra == "NOFADE" || upperExtra == "NO_FADE") {
                            action.fadeDuration = 0.0;
                        } else {
                            double f = 1.0;
                            if (TryParseDuration(extra, f)) {
                                action.fadeDuration = f;
                            }
                        }
                    }
                    frame.actions.push_back(action);
                }
            } else if (verbUpper == "STOP_VIDEO" || verbUpper == "STOPVIDEO" ||
                       verbUpper == "CLEAR_VIDEO" || verbUpper == "CLEARVIDEO") {
                AnimationAction action;
                action.type = ActionType::StopBackgroundVideo;
                frame.actions.push_back(action);
            } else if (verbUpper == "PALPITATE" || verbUpper == "PULSE" || verbUpper == "PALPITA" ||
                       verbUpper == "COLOR_CYCLE" || verbUpper == "RAINBOW") {
                AnimationAction action;
                action.type = ActionType::Palpitate;
                if (verbUpper == "RAINBOW") {
                    action.isRainbow = true;
                }
                action.minBrightness = 0.5f;
                action.startBrightness = 0.5f;
                action.maxBrightness = 1.0f;
                action.frequency = 1.2f;

                struct PctValue {
                    float value = 0.0f;
                    bool isDown = false;
                };
                std::vector<PctValue> pctList;

                std::string tok;
                while (cs >> tok) {
                    std::string tokUpper = ToUpper(tok);
                    if (tokUpper == "STOP" || tokUpper == "OFF") {
                        action.type = ActionType::StopPalpitate;
                        break;
                    }
                    if (tokUpper == "ALL" || tokUpper == "*") {
                        action.targetId = -2; // Sentinel for ALL
                        continue;
                    }
                    if (tokUpper == "RAINBOW" || tokUpper == "HUE" || tokUpper == "ARCOIRIS" || tokUpper == "SPECTRUM") {
                        action.isRainbow = true;
                        continue;
                    }
                    if (tokUpper.size() > 3 && (tokUpper.rfind("DEG") == tokUpper.size() - 3)) {
                        std::string numPart = tok.substr(0, tok.size() - 3);
                        try {
                            float deg = std::stof(numPart);
                            action.initialPhase = deg * 3.14159265358979323846f / 180.0f;
                            action.hasCustomPhase = true;
                        } catch (...) {}
                        continue;
                    }
                    size_t pctPos = tok.find('%');
                    if (pctPos != std::string::npos) {
                        std::string numPart = tok.substr(0, pctPos);
                        std::string suffix = ToUpper(tok.substr(pctPos + 1));
                        bool isDown = false;
                        if (!numPart.empty() && numPart[0] == '-') {
                            isDown = true;
                            numPart = numPart.substr(1);
                        }
                        if (suffix.find("DOWN") != std::string::npos || suffix.find("BAJA") != std::string::npos || suffix == "D") {
                            isDown = true;
                        }
                        try {
                            float pct = std::stof(numPart) / 100.0f;
                            pctList.push_back({ pct, isDown });
                        } catch (...) {}
                        continue;
                    }
                    if (tok.size() > 2 && (tokUpper.rfind("HZ") == tok.size() - 2)) {
                        std::string numPart = tok.substr(0, tok.size() - 2);
                        try {
                            action.frequency = std::stof(numPart);
                        } catch (...) {}
                        continue;
                    }
                    COLORREF parsedColor;
                    if (TryParseColor(tok, parsedColor)) {
                        action.colors.push_back(parsedColor);
                        continue;
                    }
                    if (IsDigitString(tok)) {
                        action.targetIds.push_back(std::stoi(tok));
                    } else {
                        action.targetName = tok;
                    }
                }

                if (!pctList.empty()) {
                    action.hasCustomBrightness = true;
                    if (pctList.size() == 1) {
                        action.minBrightness = 0.0f;
                        action.startBrightness = pctList[0].value;
                        action.maxBrightness = pctList[0].value;
                        action.startFalling = pctList[0].isDown;
                    } else if (pctList.size() == 2) {
                        // Two percentages: min% max%. Defaults to starting at max% (100%) and falling towards min%
                        action.minBrightness = pctList[0].value;
                        action.startBrightness = pctList[1].value;
                        action.maxBrightness = pctList[1].value;
                        action.startFalling = true;
                    } else if (pctList.size() >= 3) {
                        // min% start% max%
                        action.minBrightness = pctList[0].value;
                        action.startBrightness = pctList[1].value;
                        action.maxBrightness = pctList[2].value;
                        action.startFalling = pctList[1].isDown;
                    }

                    if (action.minBrightness > action.maxBrightness) {
                        std::swap(action.minBrightness, action.maxBrightness);
                    }
                }

                if (action.targetIds.empty() && action.targetName.empty() && action.targetId == -1) {
                    action.targetId = -2; // Default to ALL if no specific area was specified
                }

                frame.actions.push_back(action);
            } else if (verbUpper == "STOP_PALPITATE" || verbUpper == "STOPPALPITATE" ||
                       verbUpper == "STOP_PULSE" || verbUpper == "STOPPULSE") {
                AnimationAction action;
                action.type = ActionType::StopPalpitate;
                frame.actions.push_back(action);
            } else if (verbUpper == "WAIT_CLICK" || verbUpper == "WAITCLICK" ||
                       verbUpper == "WAIT_FOR_CLICK" || verbUpper == "CLICK" ||
                       verbUpper == "PAUSE_CLICK" || verbUpper == "CUE") {
                frame.waitForClick = true;
            } else if (verbUpper == "WAIT" || verbUpper == "PAUSE" || verbUpper == "SLEEP" || verbUpper == "DELAY") {
                std::string waitArg;
                if (cs >> waitArg) {
                    double waitSec = 0.0;
                    if (TryParseWaitSeconds(waitArg, waitSec)) {
                        frame.duration = waitSec;
                        frame.hasExplicitDuration = true;
                    }
                }
            } else {
                // Unknown command
                outError = "Line " + std::to_string(lineNumber) + ": Unknown command '" + verb + "'";
                return false;
            }
        }

        // If this frame only contained WAIT_CLICK and no actions, attach to previous frame if available
        if (frame.waitForClick && frame.actions.empty() && !outSequence.frames.empty()) {
            outSequence.frames.back().waitForClick = true;
            continue;
        }

        outSequence.frames.push_back(frame);
    }

    if (outSequence.frames.empty()) {
        outError = "Animation file contains no frames.";
        return false;
    }

    return true;
}
