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

if(NOT RTLSDR_LIB)
    message(WARNING "librtlsdr not found - RTL-SDR support will be disabled")
    set(RTLSDR_LIB "")
endif()

# Source files
set(SOURCES
    src/main.c
    src/rtl_interface.c
    src/tetra_demod.c
    src/tea1_crypto.c
    src/tea1_crack.c
    src/audio_output.c
    src/signal_processing.c
    src/utils.c
)

# Main executable
add_executable(tetra_analyzer ${SOURCES})

# Link libraries
target_link_libraries(tetra_analyzer
    ${RTLSDR_LIB}
    ${PTHREAD_LIB}
    ${M_LIB}
)

# Installation
install(TARGETS tetra_analyzer DESTINATION bin)
install(DIRECTORY examples/ DESTINATION share/tetra_analyzer/examples)
install(FILES README.md DESTINATION share/doc/tetra_analyzer)

# Print configuration
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C Compiler: ${CMAKE_C_COMPILER}")
message(STATUS "System: ${CMAKE_SYSTEM_NAME}")
message(STATUS "Processor: ${CMAKE_SYSTEM_PROCESSOR}")
