cmake_minimum_required(VERSION 3.16)
project(PatronAnimation CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Project include paths
set(PROJECT_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/anim
    ${CMAKE_CURRENT_SOURCE_DIR}/include/app
    ${CMAKE_CURRENT_SOURCE_DIR}/include/calib
    ${CMAKE_CURRENT_SOURCE_DIR}/include/display
    ${CMAKE_CURRENT_SOURCE_DIR}/include/media
    ${CMAKE_CURRENT_SOURCE_DIR}/include/model
    ${CMAKE_CURRENT_SOURCE_DIR}/include/render
    ${CMAKE_CURRENT_SOURCE_DIR}/include/util
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

include_directories(${PROJECT_INCLUDE_DIRS})

# Source files
set(SOURCES
    source/main.cpp
    source/display/DisplayManager.cpp
    source/render/RenderEngine.cpp
    source/app/Application.cpp
    source/model/PatternGrid.cpp
    source/model/PatternConfig.cpp
    source/calib/CalibrationController.cpp
    source/anim/AnimationParser.cpp
    source/anim/AnimationController.cpp
    source/media/VideoPlayer.cpp
)

# Headers
set(HEADERS
    include/display/DisplayManager.h
    include/render/RenderEngine.h
    include/app/Application.h
    include/model/Geometry.h
    include/model/ProjectionArea.h
    include/model/PatternGrid.h
    include/model/PatternConfig.h
    include/calib/CalibrationController.h
    include/anim/AnimationTypes.h
    include/anim/AnimationParser.h
    include/anim/AnimationController.h
    include/media/VideoPlayer.h
    include/util/StringUtil.h
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
