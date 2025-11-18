# Convenience Makefile for TETRA TEA1 Analyzer
# Wraps CMake for easier building

.PHONY: all build clean install test help arm-build

# Default target
all: build

# Create build directory and compile
build:
	@echo "Building TETRA TEA1 Analyzer..."
	@mkdir -p build
	@cd build && cmake .. && $(MAKE)
	@echo ""
	@echo "✓ Build complete!"
	@echo "Binary: ./build/tetra_analyzer"
	@echo ""
	@echo "Run './build/tetra_analyzer -h' for help"

# Optimized release build
release:
	@echo "Building optimized release version..."
	@mkdir -p build
	@cd build && cmake -DCMAKE_BUILD_TYPE=Release .. && $(MAKE)
	@echo ""
	@echo "✓ Release build complete!"

# Debug build
debug:
	@echo "Building debug version..."
	@mkdir -p build
	@cd build && cmake -DCMAKE_BUILD_TYPE=Debug .. && $(MAKE)
	@echo ""
	@echo "✓ Debug build complete!"

# ARM-optimized build
arm-build:
	@echo "Building ARM-optimized version..."
	@mkdir -p build
	@cd build && cmake -DCMAKE_BUILD_TYPE=Release .. && $(MAKE)
	@echo ""
	@echo "✓ ARM build complete!"

# Clean build artifacts
clean:
	@echo "Cleaning build artifacts..."
	@rm -rf build
	@echo "✓ Clean complete!"

# Install to system
install: build
	@echo "Installing TETRA TEA1 Analyzer..."
	@cd build && sudo $(MAKE) install
	@echo "✓ Installation complete!"
	@echo "Run 'tetra_analyzer' from anywhere"

# Uninstall from system
uninstall:
	@echo "Uninstalling TETRA TEA1 Analyzer..."
	@sudo rm -f /usr/local/bin/tetra_analyzer
	@sudo rm -rf /usr/local/share/tetra_analyzer
	@sudo rm -rf /usr/local/share/doc/tetra_analyzer
	@echo "✓ Uninstall complete!"

# Run tests
test: build
	@echo "Running in simulation mode (no hardware required)..."
	@./build/tetra_analyzer -v &
	@sleep 5
	@killall tetra_analyzer 2>/dev/null || true
	@echo "✓ Test complete!"

# Quick test run
run: build
	@./build/tetra_analyzer -v

# Show help
help:
	@echo "TETRA TEA1 Analyzer - Build System"
	@echo ""
	@echo "Available targets:"
	@echo "  make [all]      - Build the project (default)"
	@echo "  make release    - Build optimized release version"
	@echo "  make debug      - Build debug version with symbols"
	@echo "  make arm-build  - Build ARM-optimized version"
	@echo "  make clean      - Remove build artifacts"
	@echo "  make install    - Install to system (requires sudo)"
	@echo "  make uninstall  - Remove from system (requires sudo)"
	@echo "  make test       - Run basic test"
	@echo "  make run        - Build and run in simulation mode"
	@echo "  make help       - Show this help"
	@echo ""
	@echo "Examples:"
	@echo "  make            # Build normally"
	@echo "  make release    # Optimized build for ARM"
	@echo "  make run        # Quick test without hardware"
	@echo ""

# Display build info
info:
	@echo "System Information:"
	@echo "  OS: $$(uname -s)"
	@echo "  Architecture: $$(uname -m)"
	@echo "  Compiler: $$(gcc --version | head -n1)"
	@echo "  CMake: $$(cmake --version | head -n1)"
	@echo ""
	@echo "Optional Dependencies:"
	@which rtl_sdr > /dev/null && echo "  ✓ RTL-SDR tools installed" || echo "  ✗ RTL-SDR tools not found"
	@ldconfig -p | grep -q librtlsdr && echo "  ✓ librtlsdr library found" || echo "  ✗ librtlsdr library not found"
	@echo ""
	@echo "Note: RTL-SDR is optional. Simulation mode works without it."
