#!/bin/bash
# Script to install GTK3 support and rebuild TETRA Analyzer with GUI
# Run this on your Raspberry Pi Debian ARM system

set -e  # Exit on error

echo "=========================================="
echo "TETRA Analyzer GUI Support Installer"
echo "=========================================="
echo ""

# Check if running with appropriate privileges
if [ "$EUID" -eq 0 ]; then
    SUDO=""
else
    SUDO="sudo"
fi

echo "[1/4] Checking current GTK3 installation..."
if pkg-config --exists gtk+-3.0; then
    VERSION=$(pkg-config --modversion gtk+-3.0)
    echo "✓ GTK3 is already installed (version $VERSION)"
else
    echo "✗ GTK3 not found, installing..."
    echo ""
    echo "[2/4] Updating package lists..."
    $SUDO apt-get update

    echo ""
    echo "[3/4] Installing GTK3 development libraries..."
    $SUDO apt-get install -y libgtk-3-dev

    # Verify installation
    if pkg-config --exists gtk+-3.0; then
        VERSION=$(pkg-config --modversion gtk+-3.0)
        echo "✓ GTK3 installed successfully (version $VERSION)"
    else
        echo "✗ GTK3 installation failed!"
        exit 1
    fi
fi

echo ""
echo "[4/4] Rebuilding TETRA Analyzer with GUI support..."
cd "$(dirname "$0")"

# Clean previous build
make clean

# Rebuild
if make; then
    echo ""
    echo "=========================================="
    echo "✓ Build completed successfully!"
    echo "=========================================="
    echo ""

    # Check if GUI support was compiled in
    if strings ./build/tetra_analyzer | grep -q "HAVE_GTK"; then
        echo "✓ GUI support is enabled!"
    else
        # Check build output for GTK
        echo ""
        echo "Checking build configuration..."
        cd build
        cmake ..
        cd ..

        if grep -q "GTK3 found" build/CMakeFiles/CMakeOutput.log 2>/dev/null || \
           grep -q "GTK3 found" build/CMakeCache.txt 2>/dev/null; then
            echo "✓ GTK3 detected by build system"
        else
            echo "⚠ Warning: GTK3 may not have been detected by CMake"
            echo "Try running: make clean && make"
        fi
    fi

    echo ""
    echo "=========================================="
    echo "How to use the GUI:"
    echo "=========================================="
    echo ""
    echo "1. Launch with default settings:"
    echo "   ./build/tetra_analyzer -f 420000000 -G"
    echo ""
    echo "2. Launch with verbose mode:"
    echo "   ./build/tetra_analyzer -f 420000000 -G -v"
    echo ""
    echo "3. Launch with real-time audio:"
    echo "   ./build/tetra_analyzer -f 420000000 -G -r"
    echo ""
    echo "The GUI will show:"
    echo "  - Real-time signal power monitoring"
    echo "  - Adjustable detection thresholds"
    echo "  - Frequency control"
    echo "  - Detection status with visual indicator"
    echo ""
else
    echo ""
    echo "✗ Build failed! Check the error messages above."
    exit 1
fi

echo "Installation complete!"
