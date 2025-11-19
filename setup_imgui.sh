#!/bin/bash
# Script to download and setup Dear ImGui for TETRA Analyzer

set -e

echo "========================================"
echo "Dear ImGui Setup for TETRA Analyzer"
echo "========================================"
echo ""

# Check if imgui directory exists
if [ -d "external/imgui" ]; then
    echo "ImGui already exists in external/imgui"
    echo "To re-download, remove the directory first: rm -rf external/imgui"
    exit 0
fi

# Create external directory
mkdir -p external
cd external

echo "[1/3] Downloading Dear ImGui..."
# Clone ImGui
if ! git clone --depth 1 https://github.com/ocornut/imgui.git; then
    echo "Failed to download ImGui"
    exit 1
fi

echo ""
echo "[2/3] Verifying ImGui files..."

# Check required files exist
REQUIRED_FILES=(
    "imgui/imgui.cpp"
    "imgui/imgui.h"
    "imgui/imgui_draw.cpp"
    "imgui/imgui_tables.cpp"
    "imgui/imgui_widgets.cpp"
    "imgui/backends/imgui_impl_glfw.cpp"
    "imgui/backends/imgui_impl_glfw.h"
    "imgui/backends/imgui_impl_opengl3.cpp"
    "imgui/backends/imgui_impl_opengl3.h"
)

for file in "${REQUIRED_FILES[@]}"; do
    if [ ! -f "$file" ]; then
        echo "✗ Missing: $file"
        exit 1
    fi
    echo "✓ Found: $file"
done

cd ..

echo ""
echo "[3/3] Checking dependencies..."

# Check for GLFW3
if pkg-config --exists glfw3; then
    VERSION=$(pkg-config --modversion glfw3)
    echo "✓ GLFW3 found (version $VERSION)"
else
    echo "✗ GLFW3 not found"
    echo ""
    echo "Please install GLFW3:"
    echo "  sudo apt-get install libglfw3-dev"
    echo ""
    exit 1
fi

# Check for OpenGL
if pkg-config --exists gl; then
    echo "✓ OpenGL found"
elif pkg-config --exists glesv2; then
    echo "✓ OpenGL ES 2.0 found (ARM/Raspberry Pi)"
else
    echo "⚠ OpenGL libraries not found via pkg-config"
    echo "Continuing anyway - may work with system OpenGL"
fi

echo ""
echo "========================================"
echo "✓ ImGui setup complete!"
echo "========================================"
echo ""
echo "Next steps:"
echo "  1. Run: make clean && make"
echo "  2. Launch GUI: ./build/tetra_analyzer -f 420000000 -G"
echo ""
echo "The GUI will automatically use:"
echo "  - OpenGL ES 2.0 on ARM/Raspberry Pi"
echo "  - OpenGL 2.1+ on desktop systems"
echo ""
