#ifndef ANIMATION_PARSER_H
#define ANIMATION_PARSER_H

#include <string>
#include "AnimationTypes.h"

class AnimationParser {
public:
    // Parse an animation script from a file (.txt).
    static bool LoadFromFile(const std::string& path, AnimationSequence& outSequence, std::string& outError);

    // Parse an animation script from string content.
    static bool ParseString(const std::string& content, AnimationSequence& outSequence, std::string& outError);

    // Resolve an animation path (current dir, animations/, ../animations/, etc.)
    static std::string ResolvePath(const std::string& inputPath);
};

#endif // ANIMATION_PARSER_H
