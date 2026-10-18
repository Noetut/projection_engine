cmake_minimum_required(VERSION 3.16)
project(PatronAnimation CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Project include paths
set(PROJECT_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

include_directories(${PROJECT_INCLUDE_DIRS})

# Source files
set(SOURCES
    src/main.cpp
    src/DisplayManager.cpp
    src/RenderEngine.cpp
    src/Application.cpp
    src/PatternGrid.cpp
    src/PatternConfig.cpp
    src/CalibrationController.cpp
    src/AnimationParser.cpp
    src/AnimationController.cpp
    src/VideoPlayer.cpp
)

# Headers
set(HEADERS
    include/DisplayManager.h
    include/RenderEngine.h
    include/Application.h
    include/Geometry.h
    include/ProjectionArea.h
    include/PatternGrid.h
    include/PatternConfig.h
    include/CalibrationController.h
    include/AnimationTypes.h
    include/AnimationParser.h
    include/AnimationController.h
    include/VideoPlayer.h
    include/StringUtil.h
)

# Executable output
add_executable(PatronAnimation ${SOURCES} ${HEADERS})

target_include_directories(PatronAnimation
    PRIVATE
        ${PROJECT_INCLUDE_DIRS}
)

# nlohmann/json is a heavily templated single header; on MinGW the translation
# unit that includes it can exceed the default COFF section limit.
if(MINGW)
    set_source_files_properties(source/model/PatternConfig.cpp
        PROPERTIES COMPILE_OPTIONS "-Wa,-mbig-obj")
endif()

# Link Win32 libraries: gdi32 (GDI graphics), user32 (Windowing & Monitor API), msimg32 (AlphaBlend), gdiplus (Image loading & rendering), Media Foundation (video playback), winmm (precise timer)
target_link_libraries(PatronAnimation PRIVATE gdi32 user32 msimg32 gdiplus mfplat mfreadwrite mfuuid ole32 winmm)

message(STATUS "Configured PatronAnimation target for Win32 build.")
