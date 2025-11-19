cmake_minimum_required(VERSION 3.10)
project(TETRA_TEA1_Analyzer C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Optimization flags for ARM and low-resource systems
if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm.*|ARM.*|aarch64.*|AARCH64.*")
    message(STATUS "Detected ARM processor: ${CMAKE_SYSTEM_PROCESSOR}")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=native -mtune=native")
    set(ARM_DETECTED TRUE)
endif()

# Compiler optimizations
set(CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG -ffast-math -funroll-loops")
set(CMAKE_C_FLAGS_DEBUG "-O0 -g -Wall -Wextra -Wpedantic")

# Default to Release build
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# Find required libraries
find_library(RTLSDR_LIB rtlsdr)
find_library(PTHREAD_LIB pthread)
find_library(M_LIB m)
find_library(ASOUND_LIB asound)

# Find GLFW3 and OpenGL for ImGui GUI support
find_package(PkgConfig)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(GLFW3 glfw3)
endif()

# Try to find OpenGL
find_package(OpenGL)

# Check for OpenGL ES on ARM platforms
if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm.*|ARM.*|aarch64.*|AARCH64.*")
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(GLES glesv2)
    endif()
endif()

if(NOT RTLSDR_LIB)
    message(WARNING "librtlsdr not found - RTL-SDR support will be disabled")
    set(RTLSDR_LIB "")
endif()

if(NOT ASOUND_LIB)
    message(WARNING "libasound not found - Real-time audio playback will be disabled")
    message(STATUS "To enable: sudo apt-get install libasound2-dev")
    set(ASOUND_LIB "")
else()
    message(STATUS "ALSA support enabled for real-time audio playback")
    add_definitions(-DHAVE_ALSA)
endif()

# Check for ImGui
set(IMGUI_DIR "${CMAKE_SOURCE_DIR}/external/imgui")
if(EXISTS "${IMGUI_DIR}/imgui.h" AND GLFW3_FOUND AND (OPENGL_FOUND OR GLES_FOUND))
    message(STATUS "Dear ImGui found - GUI support enabled")
    add_definitions(-DHAVE_IMGUI)

    # ImGui source files
    set(IMGUI_SOURCES
        ${IMGUI_DIR}/imgui.cpp
        ${IMGUI_DIR}/imgui_draw.cpp
        ${IMGUI_DIR}/imgui_tables.cpp
        ${IMGUI_DIR}/imgui_widgets.cpp
        ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
        ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
    )

    # ImGui include directories
    include_directories(${IMGUI_DIR})
    include_directories(${IMGUI_DIR}/backends)
    include_directories(${GLFW3_INCLUDE_DIRS})

    if(GLES_FOUND)
        message(STATUS "Using OpenGL ES 2.0 for GUI (ARM/Raspberry Pi)")
        include_directories(${GLES_INCLUDE_DIRS})
    else()
        message(STATUS "Using desktop OpenGL for GUI")
    endif()

    set(HAVE_GUI TRUE)
else()
    message(WARNING "ImGui/GLFW3/OpenGL not found - GUI support will be disabled")
    message(STATUS "To enable:")
    message(STATUS "  1. Run: ./setup_imgui.sh")
    message(STATUS "  2. Install: sudo apt-get install libglfw3-dev")
    message(STATUS "  3. On ARM: sudo apt-get install libgles2-mesa-dev")
    set(HAVE_GUI FALSE)
endif()

# Source files
set(SOURCES
    src/main.c
    src/rtl_interface.c
    src/tetra_demod.c
    src/tea1_crypto.c
    src/tea1_crack.c
    src/audio_output.c
    src/audio_playback.c
    src/tetra_codec.c
    src/signal_processing.c
    src/utils.c
    src/trunking.c
    src/control_channel.c
)

# Add GUI sources if ImGui is available
if(HAVE_GUI)
    list(APPEND SOURCES src/imgui_gui.cpp)
    list(APPEND SOURCES ${IMGUI_SOURCES})
endif()

# Main executable
add_executable(tetra_analyzer ${SOURCES})

# Link libraries
target_link_libraries(tetra_analyzer
    ${RTLSDR_LIB}
    ${PTHREAD_LIB}
    ${M_LIB}
    ${ASOUND_LIB}
)

# Link ImGui/GLFW/OpenGL libraries if available
if(HAVE_GUI)
    target_link_libraries(tetra_analyzer ${GLFW3_LIBRARIES})

    # Link OpenGL or OpenGL ES
    if(GLES_FOUND)
        target_link_libraries(tetra_analyzer ${GLES_LIBRARIES})
        message(STATUS "Linking OpenGL ES 2.0")
    elseif(OPENGL_FOUND)
        target_link_libraries(tetra_analyzer ${OPENGL_LIBRARIES})
        message(STATUS "Linking desktop OpenGL")
    endif()

    # Additional libraries needed on some platforms
    find_library(DL_LIB dl)
    if(DL_LIB)
        target_link_libraries(tetra_analyzer ${DL_LIB})
    endif()
endif()

# Installation
install(TARGETS tetra_analyzer DESTINATION bin)
install(DIRECTORY examples/ DESTINATION share/tetra_analyzer/examples)
install(FILES README.md DESTINATION share/doc/tetra_analyzer)

# Print configuration
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C Compiler: ${CMAKE_C_COMPILER}")
message(STATUS "System: ${CMAKE_SYSTEM_NAME}")
message(STATUS "Processor: ${CMAKE_SYSTEM_PROCESSOR}")
