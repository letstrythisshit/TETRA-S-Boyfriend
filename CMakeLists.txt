cmake_minimum_required(VERSION 3.10)
project(TETRA_TEA1_Analyzer C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

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

# Find GTK3 for GUI support
find_package(PkgConfig)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(GTK3 gtk+-3.0)
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

if(GTK3_FOUND)
    message(STATUS "GTK3 found - GUI support enabled")
    add_definitions(-DHAVE_GTK)
    include_directories(${GTK3_INCLUDE_DIRS})
    link_directories(${GTK3_LIBRARY_DIRS})
    add_definitions(${GTK3_CFLAGS_OTHER})
else()
    message(WARNING "GTK3 not found - GUI support will be disabled")
    message(STATUS "To enable: sudo apt-get install libgtk-3-dev")
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

# Add GUI source if GTK3 is available
if(GTK3_FOUND)
    list(APPEND SOURCES src/gui.c)
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

# Link GTK3 libraries if available
if(GTK3_FOUND)
    target_link_libraries(tetra_analyzer ${GTK3_LIBRARIES})
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
