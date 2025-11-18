# TETRA TEA1 Educational Security Research Toolkit

![License](https://img.shields.io/badge/license-Educational-blue)
![Platform](https://img.shields.io/badge/platform-ARM%20%7C%20x86-green)
![Language](https://img.shields.io/badge/language-C-orange)

## ⚠️ EDUCATIONAL USE ONLY

This software is a **demonstrative and educational toolkit** for understanding the publicly disclosed vulnerabilities in TETRA TEA1 encryption. It is designed for:

- ✅ Security research and education
- ✅ Laboratory testing on equipment you own or control
- ✅ Academic study of cryptographic vulnerabilities
- ✅ Authorized penetration testing with explicit permission

**NOT** for:
- ❌ Unauthorized interception of communications
- ❌ Any illegal activities
- ❌ Production or malicious use

## 📖 Overview

This toolkit demonstrates the TEA1 encryption vulnerabilities in TETRA (Terrestrial Trunked Radio) systems as documented in academic security research, specifically the "TETRA:BURST" research by Midnight Blue (2023).

### Key Features

- **RTL-SDR Integration**: Capture TETRA signals using RTL-SDR hardware
- **Signal Demodulation**: TETRA π/4-DQPSK demodulation and burst detection
- **TEA1 Decryption**: Implementation of TEA1 cipher with known vulnerabilities
- **Key Recovery**: Demonstrates 32-bit keyspace reduction vulnerability
- **Audio Output**: Extract and save decrypted audio streams
- **ARM Optimized**: Designed for low-resource ARM devices (Raspberry Pi, etc.)
- **Educational**: Well-commented code explaining each vulnerability

## 🔬 The TEA1 Vulnerability

TEA1 (TETRA Encryption Algorithm 1) was found to have critical weaknesses:

1. **Reduced Keyspace**: Due to weak key schedule, the effective key size is only 32 bits instead of the claimed 80 bits
2. **Known Plaintext Attacks**: TETRA protocol structure enables known plaintext attacks
3. **Backdoor Suspicions**: The weakness appears deliberate rather than accidental

This reduces cracking time from centuries to **minutes or hours** on modern hardware.

### Timeline
- **1995**: TEA1 designed (classified)
- **2023**: Vulnerabilities publicly disclosed by security researchers
- **Present**: This educational toolkit for learning about the flaws

## 🛠️ Building and Installation

### Requirements

#### Software
- GCC or Clang compiler
- CMake 3.10 or higher
- librtlsdr (optional, for real hardware)
- pthread library
- Standard C math library

#### Hardware (Optional)
- RTL-SDR dongle (RTL2832U-based USB receiver)
- Antenna suitable for TETRA frequencies (380-470 MHz)
- Laboratory TETRA transmitter for testing

### Compilation

#### Standard Build
```bash
mkdir build
cd build
cmake ..
make
```

#### ARM Optimized Build (Raspberry Pi)
```bash
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```

#### Cross-Compilation for ARM
```bash
mkdir build
cd build
cmake -DCMAKE_TOOLCHAIN_FILE=../arm-toolchain.cmake ..
make
```

### Installation
```bash
sudo make install
```

This installs:
- `tetra_analyzer` binary to `/usr/local/bin`
- Documentation to `/usr/local/share/doc/tetra_analyzer`
- Examples to `/usr/local/share/tetra_analyzer/examples`

## 🚀 Usage

### Basic Usage

Listen for TETRA signals on 420 MHz:
```bash
./tetra_analyzer -f 420000000 -v
```

### Advanced Usage

Enable vulnerability exploitation and save audio:
```bash
./tetra_analyzer -f 420000000 -k -o audio.wav -v
```

Run with specific gain settings:
```bash
./tetra_analyzer -f 420000000 -g 30 -v
```

### Command Line Options

```
Options:
  -f, --frequency FREQ   Frequency in Hz (default: 420000000)
  -s, --sample-rate SR   Sample rate (default: 2400000)
  -g, --gain GAIN        Tuner gain in dB (default: auto)
  -d, --device INDEX     RTL-SDR device index (default: 0)
  -o, --output FILE      Output audio file (WAV format)
  -v, --verbose          Verbose output
  -k, --use-vulnerability Use known TEA1 vulnerability
  -h, --help             Show help message
```

## 📊 Memory and Performance

Optimized for low-resource ARM systems:

| Device | RAM Usage | CPU Usage | Performance |
|--------|-----------|-----------|-------------|
| Raspberry Pi 3 | ~50 MB | ~40% | Real-time |
| Raspberry Pi 4 | ~50 MB | ~25% | Real-time |
| Desktop x86_64 | ~50 MB | ~10% | Real-time |

### Optimization Features
- Efficient buffer management (256KB SDR buffer)
- Fixed-point arithmetic where applicable
- NEON vectorization hints for ARM
- Minimal memory allocations
- O3 compiler optimization

## 🔐 Security Research Context

This toolkit is based on the following public research:

### Primary Research
- **TETRA:BURST** - Midnight Blue (2023)
  - Discovered TEA1 keyspace reduction vulnerability
  - Documented backdoor characteristics
  - Published proof-of-concept attacks

### Additional Resources
- ETSI TETRA specifications (public documentation)
- Academic papers on TETRA security
- DEF CON and Black Hat presentations

## 📚 Documentation

### Code Structure
```
TETRA-S-Boyfriend/
├── src/
│   ├── main.c              # Main application and CLI
│   ├── rtl_interface.c     # RTL-SDR hardware interface
│   ├── tetra_demod.c       # TETRA demodulation
│   ├── tea1_crypto.c       # TEA1 encryption/decryption
│   ├── tea1_crack.c        # Vulnerability exploitation
│   ├── signal_processing.c # DSP functions
│   ├── audio_output.c      # Audio stream handling
│   └── utils.c             # Helper functions
├── include/
│   └── tetra_analyzer.h    # Main header file
├── docs/
│   ├── ARCHITECTURE.md     # System architecture
│   ├── TEA1_VULNERABILITY.md # Vulnerability details
│   └── API.md              # API documentation
├── examples/
│   └── example_config.conf # Example configuration
└── CMakeLists.txt          # Build configuration
```

### Key Modules

1. **RTL-SDR Interface** (`rtl_interface.c`)
   - Hardware abstraction layer
   - I/Q sample capture
   - Simulation mode for testing without hardware

2. **TETRA Demodulator** (`tetra_demod.c`)
   - π/4-DQPSK demodulation
   - Training sequence detection
   - Burst synchronization

3. **TEA1 Cryptography** (`tea1_crypto.c`)
   - TEA1 cipher implementation
   - Vulnerability demonstration
   - Key schedule analysis

4. **Key Recovery** (`tea1_crack.c`)
   - 32-bit keyspace brute force
   - Known plaintext attacks
   - Performance benchmarking

## 🧪 Testing

The toolkit includes simulation mode for testing without hardware:

```bash
# Run in simulation mode (no RTL-SDR required)
./tetra_analyzer -v
```

This generates synthetic TETRA-like signals for testing the demodulation and decryption pipeline.

## 🤝 Contributing

This is an educational project. Contributions are welcome for:
- Documentation improvements
- Code optimization
- Additional vulnerability demonstrations
- Educational examples

Please ensure all contributions maintain the educational focus and ethical use guidelines.

## ⚖️ Legal Notice

This software is provided for **educational and authorized security research purposes only**.

- Using this software to intercept communications without authorization is illegal in most jurisdictions
- Only use on laboratory equipment or with explicit written permission
- The authors assume no liability for misuse of this software
- This software is not intended for production use

### Responsible Disclosure
If you discover new vulnerabilities in TETRA or related systems:
1. Do not exploit them maliciously
2. Report to appropriate authorities (ETSI, vendors, etc.)
3. Follow coordinated disclosure practices
4. Allow time for patches before public disclosure

## 📄 License

Educational and Research Use Only

Copyright (c) 2024 TETRA Security Research Project

Permission is granted to use this software for educational purposes,
security research, and authorized testing only.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND.

## 🔗 References

1. Midnight Blue (2023) - "TETRA:BURST: Breaking TETRA" - https://www.midnight-blue.com/
2. ETSI EN 300 392-7 - TETRA Security Specification
3. Academic papers on TETRA security analysis
4. RTL-SDR Documentation - https://www.rtl-sdr.com/

## 📞 Contact

For educational inquiries or responsible disclosure:
- Open an issue on the project repository
- Follow coordinated disclosure practices
- Respect ethical guidelines

---

**Remember**: Use this knowledge to make systems more secure, not to exploit them.

*"With great power comes great responsibility"*
