/*
 * Signal Processing Module
 * Low-level DSP functions optimized for ARM processors
 */

#include "tetra_analyzer.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

void convert_uint8_to_float(const uint8_t *input, float *output, uint32_t len) {
    // Optimized conversion with ARM NEON hints (compiler will vectorize)
    for (uint32_t i = 0; i < len; i++) {
        output[i] = (float)input[i];
    }
}

void quadrature_demod(const float *i, const float *q, float *output, uint32_t len) {
    // FM quadrature demodulation: arctan(Q/I) differentiation
    // Simplified implementation using atan2

    float prev_phase = 0.0f;

    for (uint32_t n = 0; n < len; n++) {
        // Calculate instantaneous phase
        float phase = atan2f(q[n], i[n]);

        // Calculate phase difference (frequency)
        float diff = phase - prev_phase;

        // Unwrap phase (handle discontinuities)
        if (diff > M_PI) {
            diff -= 2.0f * M_PI;
        } else if (diff < -M_PI) {
            diff += 2.0f * M_PI;
        }

        output[n] = diff;
        prev_phase = phase;
    }
}

void low_pass_filter(float *data, uint32_t len, float cutoff) {
    // Simple IIR low-pass filter (optimized for low resources)
    // Uses exponential moving average
    // alpha = cutoff frequency (0.0 to 1.0)

    if (len < 2) return;

    float alpha = cutoff;
    float prev = data[0];

    for (uint32_t i = 1; i < len; i++) {
        data[i] = alpha * data[i] + (1.0f - alpha) * prev;
        prev = data[i];
    }
}

float detect_signal_strength(const float *i, const float *q, uint32_t len) {
    // Calculate average signal power
    float power = 0.0f;

    for (uint32_t n = 0; n < len; n++) {
        power += (i[n] * i[n] + q[n] * q[n]);
    }

    return sqrtf(power / len);
}

// Additional DSP utilities

void downsample(const float *input, float *output, uint32_t input_len, uint32_t factor) {
    // Simple decimation by factor
    uint32_t output_idx = 0;

    for (uint32_t i = 0; i < input_len; i += factor) {
        output[output_idx++] = input[i];
    }
}

void apply_window(float *data, uint32_t len, int window_type) {
    // Apply windowing function (Hamming window)
    for (uint32_t i = 0; i < len; i++) {
        float w = 0.54f - 0.46f * cosf(2.0f * M_PI * i / (len - 1));
        data[i] *= w;
    }
}
