#!/bin/bash
# Example usage scripts for TETRA TEA1 Analyzer
# Educational demonstrations

echo "╔═══════════════════════════════════════════════════════════════╗"
echo "║   TETRA TEA1 Analyzer - Example Usage Scripts                ║"
echo "║   Educational Security Research Toolkit                       ║"
echo "╚═══════════════════════════════════════════════════════════════╝"
echo ""

# Check if binary exists
if [ ! -f "../build/tetra_analyzer" ]; then
    echo "Error: tetra_analyzer not found. Please build first:"
    echo "  mkdir build && cd build && cmake .. && make"
    exit 1
fi

ANALYZER="../build/tetra_analyzer"

# Example 1: Basic signal listening
example1() {
    echo "=== Example 1: Basic Signal Listening ==="
    echo "Listen for TETRA signals on 420 MHz with verbose output"
    echo ""
    echo "Command: $ANALYZER -f 420000000 -v"
    echo ""
    echo "Press Ctrl+C to stop"
    echo ""
    $ANALYZER -f 420000000 -v
}

# Example 2: Vulnerability exploitation
example2() {
    echo "=== Example 2: TEA1 Vulnerability Exploitation ==="
    echo "Enable key cracking using known 32-bit keyspace vulnerability"
    echo ""
    echo "Command: $ANALYZER -f 420000000 -k -v"
    echo ""
    $ANALYZER -f 420000000 -k -v
}

# Example 3: Save audio output
example3() {
    echo "=== Example 3: Save Decrypted Audio ==="
    echo "Capture and save audio to WAV file"
    echo ""
    OUTPUT_FILE="tetra_audio_$(date +%Y%m%d_%H%M%S).wav"
    echo "Command: $ANALYZER -f 420000000 -k -o $OUTPUT_FILE -v"
    echo ""
    $ANALYZER -f 420000000 -k -o "$OUTPUT_FILE" -v

    if [ -f "$OUTPUT_FILE" ]; then
        echo ""
        echo "Audio saved to: $OUTPUT_FILE"
        ls -lh "$OUTPUT_FILE"
    fi
}

# Example 4: Custom frequency and gain
example4() {
    echo "=== Example 4: Custom Frequency and Gain ==="
    echo "Scan at 425 MHz with manual gain of 30 dB"
    echo ""
    echo "Command: $ANALYZER -f 425000000 -g 30 -v"
    echo ""
    $ANALYZER -f 425000000 -g 30 -v
}

# Example 5: Multiple device support
example5() {
    echo "=== Example 5: Multiple RTL-SDR Devices ==="
    echo "Use second RTL-SDR device (device index 1)"
    echo ""
    echo "Command: $ANALYZER -f 420000000 -d 1 -v"
    echo ""
    $ANALYZER -f 420000000 -d 1 -v
}

# Example 6: Simulation mode
example6() {
    echo "=== Example 6: Simulation Mode (No Hardware) ==="
    echo "Run without RTL-SDR hardware for testing"
    echo "This will automatically use simulation mode if no device found"
    echo ""
    echo "Command: $ANALYZER -v"
    echo ""
    $ANALYZER -v
}

# Menu
show_menu() {
    echo ""
    echo "Select an example to run:"
    echo "  1) Basic signal listening (420 MHz)"
    echo "  2) TEA1 vulnerability exploitation"
    echo "  3) Save decrypted audio to file"
    echo "  4) Custom frequency and gain (425 MHz, 30 dB)"
    echo "  5) Use second RTL-SDR device"
    echo "  6) Simulation mode (no hardware required)"
    echo "  q) Quit"
    echo ""
    read -p "Choice: " choice

    case $choice in
        1) example1 ;;
        2) example2 ;;
        3) example3 ;;
        4) example4 ;;
        5) example5 ;;
        6) example6 ;;
        q|Q) echo "Goodbye!"; exit 0 ;;
        *) echo "Invalid choice"; show_menu ;;
    esac

    show_menu
}

# Run menu if no arguments, otherwise run specific example
if [ $# -eq 0 ]; then
    show_menu
else
    case $1 in
        1|2|3|4|5|6) example$1 ;;
        *) echo "Usage: $0 [1-6]"; exit 1 ;;
    esac
fi
