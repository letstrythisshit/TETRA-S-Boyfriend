# Quick Start Guide

Get started with the TETRA TEA1 Educational Security Research Toolkit in minutes!

## Installation

### 1. Install Dependencies (Optional)

For real RTL-SDR hardware support:

```bash
# Ubuntu/Debian
sudo apt-get update
sudo apt-get install librtlsdr-dev cmake build-essential

# Raspberry Pi
sudo apt-get update
sudo apt-get install rtl-sdr librtlsdr-dev cmake build-essential

# Fedora/RHEL
sudo dnf install rtl-sdr-devel cmake gcc
```

**Note**: The software works in simulation mode without RTL-SDR hardware.

### 2. Build

```bash
# Clone or extract the repository
cd TETRA-S-Boyfriend

# Build using make
make build

# Or build manually with cmake
mkdir build && cd build
cmake ..
make
```

Build time: ~30 seconds on modern hardware

## First Run

### Test Without Hardware (Simulation Mode)

The easiest way to test the software:

```bash
./build/tetra_analyzer -v
```

This runs in **simulation mode** - no hardware needed! It generates synthetic TETRA-like signals for testing.

Expected output:
```
╔═══════════════════════════════════════════════════════════════╗
║   TETRA TEA1 Educational Security Research Toolkit v1.0.0   ║
║   ...Educational demonstration...                             ║
╚═══════════════════════════════════════════════════════════════╝

Initializing TETRA analyzer...
Frequency: 420000000 Hz (420.000 MHz)
Sample Rate: 2400000 Hz
No RTL-SDR devices found.

Running in SIMULATION mode - generating test TETRA signals
...
```

Press **Ctrl+C** to stop.

### With RTL-SDR Hardware

If you have RTL-SDR hardware connected:

```bash
# Basic listening on 420 MHz
./build/tetra_analyzer -f 420000000 -v

# With vulnerability exploitation enabled
./build/tetra_analyzer -f 420000000 -k -v

# Save audio to file
./build/tetra_analyzer -f 420000000 -k -o output.wav -v
```

## Common Use Cases

### 1. Educational Demonstration

Show students/colleagues how the vulnerability works:

```bash
# Enable verbose output and vulnerability mode
./build/tetra_analyzer -f 420000000 -k -v
```

### 2. Laboratory Testing

Test with your own TETRA transmitter in a Faraday cage:

```bash
# Scan specific frequency with manual gain
./build/tetra_analyzer -f 425000000 -g 30 -k -v
```

### 3. Security Research

Analyze and document the vulnerabilities:

```bash
# Capture session with audio output
./build/tetra_analyzer -f 420000000 -k -o research_$(date +%Y%m%d).wav -v
```

## Understanding the Output

### Normal Listening Mode

```
Initializing TETRA analyzer...
Frequency: 420000000 Hz (420.000 MHz)
Sample Rate: 2400000 Hz
RTL-SDR initialized successfully
Starting SDR capture...
Press Ctrl+C to stop

TETRA burst detected!
Training sequence found at offset 45 (20/22 matches)
```

### Vulnerability Exploitation Mode (`-k` flag)

```
⚠️  TEA1 vulnerability exploitation ENABLED
Using reduced 32-bit keyspace attack

TETRA burst detected!
Encrypted: 4F 7A 82 B3 C5 D1 E8 F2
Decrypted: 48 65 6C 6C 6F 21 00 00

=== TEA1 32-bit Keyspace Brute Force ===
Exploiting known vulnerability to reduce keyspace
Testing 1000000 candidate keys...
Progress: 100000 keys tested (250000 keys/sec)
✓ KEY FOUND! 0x12345678 after 234567 attempts
```

## Troubleshooting

### Problem: "No RTL-SDR devices found"

**Solution**: This is normal! The software will run in simulation mode. To use real hardware:
1. Connect RTL-SDR dongle
2. Install librtlsdr: `sudo apt-get install librtlsdr-dev`
3. Verify device: `rtl_test`
4. Rebuild: `make clean && make build`

### Problem: "Permission denied" accessing RTL-SDR

**Solution**: Add udev rules or run with sudo (not recommended):

```bash
# Better: Add udev rules
sudo cp /path/to/rtl-sdr.rules /etc/udev/rules.d/
sudo udevadm control --reload-rules

# Quick fix (less secure): Use sudo
sudo ./build/tetra_analyzer -f 420000000 -v
```

### Problem: Build fails with CMake errors

**Solution**: Ensure CMake 3.10+:

```bash
cmake --version  # Should be 3.10 or higher
sudo apt-get install cmake  # Update if needed
```

### Problem: High CPU usage

**Solution**: The software is optimized but signal processing is CPU-intensive:
- On ARM devices: Normal to use 30-50% CPU
- Reduce sample rate: `-s 1200000` (may affect performance)
- Close other applications

## Performance Tips

### Raspberry Pi Optimization

```bash
# Build with release optimizations (default)
make release

# Overclock (if needed, at your own risk)
# Edit /boot/config.txt, add: arm_freq=1800

# Use heat sink and fan for sustained operation
```

### Low Memory Systems

The software is designed for low memory (~50MB), but if you need to optimize further:

- Close unnecessary services
- Reduce buffer sizes in `tetra_analyzer.h` (requires rebuild)
- Use swap file if needed

## Next Steps

1. **Read the Full README**: `less README.md`
2. **Explore Architecture**: `less docs/ARCHITECTURE.md`
3. **Understand Vulnerability**: `less docs/TEA1_VULNERABILITY.md`
4. **Try Examples**: `./examples/example_usage.sh`

## Safety Reminders

⚠️ **IMPORTANT**:

- ✅ Use only on equipment you own/control
- ✅ Laboratory and educational use only
- ✅ Obtain proper authorization for testing
- ❌ Never intercept unauthorized communications
- ❌ Respect privacy and laws

## Getting Help

- Check documentation: `docs/` directory
- Review examples: `examples/` directory
- Run help: `./build/tetra_analyzer -h`

## Quick Reference

| Command | Purpose |
|---------|---------|
| `make build` | Compile the software |
| `make clean` | Remove build files |
| `make test` | Run quick test |
| `./build/tetra_analyzer -h` | Show help |
| `./build/tetra_analyzer -v` | Run in simulation mode |
| `./build/tetra_analyzer -f FREQ -v` | Listen on frequency |
| `./build/tetra_analyzer -k -v` | Enable vulnerability exploitation |

---

**Happy Learning!** Remember: Use this knowledge ethically to improve security, not exploit it.
