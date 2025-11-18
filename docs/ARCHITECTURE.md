# TETRA Analyzer Architecture

## System Overview

The TETRA TEA1 Analyzer is designed as a modular pipeline processing system optimized for low-resource ARM devices.

```
┌─────────────┐     ┌──────────────┐     ┌─────────────┐
│  RTL-SDR    │────▶│   TETRA      │────▶│    TEA1     │
│  Hardware   │     │ Demodulator  │     │  Decryption │
└─────────────┘     └──────────────┘     └─────────────┘
      │                    │                     │
      │ I/Q Samples        │ Demod Bits          │ Plaintext
      ▼                    ▼                     ▼
┌─────────────┐     ┌──────────────┐     ┌─────────────┐
│   Signal    │     │   Burst      │     │    Audio    │
│  Processing │     │  Detection   │     │   Output    │
└─────────────┘     └──────────────┘     └─────────────┘
```

## Module Descriptions

### 1. RTL-SDR Interface (`rtl_interface.c`)

**Purpose**: Hardware abstraction and I/Q sample acquisition

**Functions**:
- Device initialization and configuration
- Frequency tuning (380-470 MHz TETRA band)
- Gain control (auto/manual)
- Continuous sample streaming
- Simulation mode for testing

**Buffer Management**:
- 256KB circular buffer (optimized for low memory)
- Zero-copy where possible
- Callback-based architecture

### 2. Signal Processing (`signal_processing.c`)

**Purpose**: Low-level DSP operations

**Algorithms**:
- **Quadrature Demodulation**: FM detection using atan2 and phase differentiation
- **Low-Pass Filtering**: Simple IIR filter for noise reduction
- **Signal Strength Detection**: Power measurement for squelch
- **Format Conversion**: uint8 to float with DC removal

**ARM Optimizations**:
- NEON vectorization hints
- Fast math approximations
- Minimal branching

### 3. TETRA Demodulator (`tetra_demod.c`)

**Purpose**: TETRA-specific signal demodulation

**Features**:
- π/4-DQPSK demodulation
- Symbol timing recovery
- Training sequence detection (22-bit pattern)
- Burst synchronization
- Bit extraction

**Constants**:
- Symbol rate: 18 kHz
- Samples per symbol: ~133 @ 2.4 MHz sample rate
- Burst length: 510 symbols
- Training sequence: Known 22-bit pattern

**Detection Algorithm**:
```
For each potential burst position:
  1. Correlate with known training sequence
  2. If correlation > threshold (18/22 bits):
     - Mark as valid burst
     - Extract payload bits
     - Pass to decryption module
```

### 4. TEA1 Cryptography (`tea1_crypto.c`)

**Purpose**: TEA1 cipher implementation with vulnerabilities

**Cipher Specifications**:
- Block size: 64 bits (8 bytes)
- Key size: 80 bits (10 bytes) - nominal
- Effective key size: 32 bits (due to vulnerability)
- Rounds: 32
- Mode: CBC-like stream mode

**Vulnerability Implementation**:
The key schedule weakness is implemented to demonstrate the vulnerability:

```c
// Only first 32 bits are effective
uint32_t effective_key = key[0..3];
// Remaining 48 bits (key[4..9]) ignored due to weak schedule
```

**Round Function**:
1. S-box substitution (8-bit × 4)
2. XOR with round key
3. Rotation/permutation

### 5. TEA1 Key Recovery (`tea1_crack.c`)

**Purpose**: Vulnerability exploitation and key recovery

**Attack Methods**:

#### A. Brute Force 32-bit Keyspace
```
for candidate_key in 0 to 2^32:
  test_decrypt(ciphertext, candidate_key)
  if result == known_plaintext:
    return candidate_key
```

**Complexity**: 2^32 operations (~4 billion)
**Time**: Minutes to hours on modern CPU

#### B. Known Plaintext Attack
Exploits predictable TETRA protocol structure:
- Training sequences (always same pattern)
- Synchronization bursts
- System information (color codes)
- Null encryption frames

**Success Rate**: High for active TETRA networks

### 6. Audio Output (`audio_output.c`)

**Purpose**: Decoded audio stream handling

**Features**:
- WAV file output (16-bit PCM)
- Configurable sample rate (typically 8 kHz)
- Buffered writing for efficiency
- Proper WAV header generation

## Data Flow

### Normal Operation Flow

```
1. RTL-SDR captures RF @ 420 MHz
   ↓
2. Hardware downconverts to baseband I/Q
   ↓
3. ADC samples @ 2.4 MHz → uint8 I/Q pairs
   ↓
4. Convert to float, remove DC bias
   ↓
5. Quadrature demod → instantaneous frequency
   ↓
6. Low-pass filter @ 18 kHz
   ↓
7. Symbol timing recovery
   ↓
8. Sample at symbol rate → binary bits
   ↓
9. Search for training sequence
   ↓
10. Extract encrypted payload
    ↓
11. TEA1 decrypt (with or without cracking)
    ↓
12. Output audio/data
```

### Memory Usage by Module

| Module | Allocation | Purpose |
|--------|------------|---------|
| RTL-SDR | 256 KB | I/Q sample buffer |
| Demodulator | ~50 KB | Float sample buffers |
| TEA1 | < 1 KB | Cipher state |
| Audio | 16 KB | Output buffer |
| **Total** | **~320 KB** | **Peak usage** |

## Performance Characteristics

### CPU Usage Breakdown
- RTL-SDR I/O: 10%
- Signal processing: 30%
- Demodulation: 40%
- Decryption: 15%
- Overhead: 5%

### Optimization Strategies

1. **Fixed-Point Math**: Where floating-point precision not needed
2. **Lookup Tables**: For trigonometric functions
3. **SIMD Hints**: Let compiler vectorize loops
4. **Minimal Allocations**: Pre-allocate buffers at startup
5. **Efficient Algorithms**: O(n) where possible, avoid O(n²)

## Threading Model

Current implementation is single-threaded for simplicity. Potential multi-threading:

```
Thread 1: RTL-SDR I/O (producer)
   ↓ (lock-free queue)
Thread 2: Signal processing + demodulation
   ↓ (lock-free queue)
Thread 3: Decryption + audio output
```

## Error Handling

- **Hardware Errors**: Graceful fallback to simulation mode
- **Signal Loss**: Continue scanning
- **Decryption Failures**: Log and continue
- **Resource Exhaustion**: Clean shutdown

## Future Enhancements

1. **Multi-channel Support**: Monitor multiple frequencies
2. **GPU Acceleration**: Use OpenCL for signal processing
3. **Better Timing Recovery**: Gardner/Mueller-Müller algorithms
4. **FEC Decoding**: Reed-Solomon error correction
5. **Protocol Decoding**: Full TETRA stack parsing

## Security Considerations

This architecture is designed for **educational demonstration only**:

- Simulated data mode for safe testing
- Clear warnings about authorized use
- No stealth features
- Extensive logging for transparency

## References

- ETSI EN 300 392-2: TETRA Air Interface
- Digital Signal Processing (Oppenheim & Schafer)
- Software Defined Radio fundamentals
