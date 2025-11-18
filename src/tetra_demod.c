/*
 * TETRA Demodulation Module
 * Implements TETRA signal detection and demodulation
 */

#include "tetra_analyzer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// TETRA training sequence for burst detection
static const uint8_t TETRA_TRAINING_SEQ[22] = {
    1, 1, 0, 0, 1, 0, 1, 0, 0, 1, 1, 1, 0, 1, 0, 0, 0, 1, 1, 0, 1, 0
};

tetra_demod_t* tetra_demod_init(uint32_t sample_rate) {
    tetra_demod_t *demod = calloc(1, sizeof(tetra_demod_t));
    if (!demod) {
        fprintf(stderr, "Failed to allocate demodulator structure\n");
        return NULL;
    }

    demod->sample_count = SDR_BUFFER_SIZE / 2; // I/Q pairs

    // Allocate sample buffers (optimized for low memory)
    demod->i_samples = calloc(demod->sample_count, sizeof(float));
    demod->q_samples = calloc(demod->sample_count, sizeof(float));
    demod->demod_bits = calloc(TETRA_BURST_LENGTH, sizeof(uint8_t));

    if (!demod->i_samples || !demod->q_samples || !demod->demod_bits) {
        fprintf(stderr, "Failed to allocate demodulator buffers\n");
        tetra_demod_cleanup(demod);
        return NULL;
    }

    demod->bit_count = 0;
    demod->symbol_timing = 0.0f;

    return demod;
}

int tetra_demod_process(tetra_demod_t *demod, uint8_t *iq_data, uint32_t len) {
    if (!demod || !iq_data || len < 2) {
        return -1;
    }

    // Convert uint8 I/Q samples to float and separate I/Q
    uint32_t sample_pairs = len / 2;
    if (sample_pairs > (uint32_t)demod->sample_count) {
        sample_pairs = demod->sample_count;
    }

    // Convert and separate I/Q in one pass to avoid buffer issues
    for (uint32_t i = 0; i < sample_pairs; i++) {
        demod->i_samples[i] = (float)iq_data[i * 2] - 127.5f;
        demod->q_samples[i] = (float)iq_data[i * 2 + 1] - 127.5f;
    }

    // Perform quadrature demodulation
    float *demod_output = malloc(sample_pairs * sizeof(float));
    if (!demod_output) return -1;

    quadrature_demod(demod->i_samples, demod->q_samples, demod_output, sample_pairs);

    // Apply low-pass filter (strengthened from 0.1f to 0.5f for better noise rejection)
    low_pass_filter(demod_output, sample_pairs, 0.5f);

    // Symbol timing recovery and bit extraction (simplified)
    // Real implementation would use Gardner or Mueller-Müller timing recovery
    float samples_per_symbol = 2400000.0f / 18000.0f; // ~133 samples/symbol
    int bit_index = 0;

    for (uint32_t i = 0; i < sample_pairs && bit_index < TETRA_BURST_LENGTH; i += (int)samples_per_symbol) {
        // Simple threshold detection
        demod->demod_bits[bit_index++] = (demod_output[i] > 0.0f) ? 1 : 0;
    }

    demod->bit_count = bit_index;

    free(demod_output);
    return bit_index;
}

bool tetra_detect_burst(tetra_demod_t *demod) {
    if (!demod || demod->bit_count < 22) {
        return false;
    }

    // Step 1: Check signal power to reject pure noise
    // Calculate RMS power from I/Q samples
    float signal_power = detect_signal_strength(demod->i_samples, demod->q_samples,
                                                 demod->sample_count);

    // Minimum signal power threshold (tuned for TETRA signals)
    // Typical TETRA signal shows power > 10.0, noise usually < 5.0
    const float MIN_SIGNAL_POWER = 8.0f;

    if (signal_power < MIN_SIGNAL_POWER) {
        log_message(true, "Signal power too low: %.2f < %.2f (rejecting noise)\n",
                   signal_power, MIN_SIGNAL_POWER);
        return false;
    }

    // Step 2: Search for training sequence in demodulated bits
    int best_match = 0;
    int best_offset = -1;
    float best_correlation = 0.0f;

    for (int offset = 0; offset < demod->bit_count - 22; offset++) {
        int matches = 0;
        float correlation = 0.0f;

        // Count bit matches and calculate correlation
        for (int i = 0; i < 22; i++) {
            if (demod->demod_bits[offset + i] == TETRA_TRAINING_SEQ[i]) {
                matches++;
                correlation += 1.0f;
            } else {
                correlation -= 1.0f;
            }
        }

        // Normalize correlation to [-1.0, 1.0]
        correlation /= 22.0f;

        if (matches > best_match) {
            best_match = matches;
            best_offset = offset;
            best_correlation = correlation;
        }

        // Strong match threshold: >= 20 of 22 bits (90.9% accuracy)
        // This significantly reduces false positives from noise
        if (matches >= 20 && correlation >= 0.8f) {
            log_message(true, "TETRA burst detected at offset %d (%d/22 matches, corr=%.3f, power=%.2f)\n",
                       offset, matches, correlation, signal_power);
            return true;
        }
    }

    // Moderate detection threshold: >= 19 of 22 bits (86.4% accuracy)
    // Only accept if correlation is strong and signal power is good
    if (best_match >= 19 && best_correlation >= 0.75f && signal_power >= MIN_SIGNAL_POWER * 1.2f) {
        log_message(true, "TETRA burst detected (moderate) at offset %d (%d/22 matches, corr=%.3f, power=%.2f)\n",
                   best_offset, best_match, best_correlation, signal_power);
        return true;
    }

    // Log rejection for debugging
    if (best_match >= 15) {
        log_message(true, "Rejected: insufficient quality (matches=%d/22, corr=%.3f, power=%.2f)\n",
                   best_match, best_correlation, signal_power);
    }

    return false;
}

void tetra_demod_cleanup(tetra_demod_t *demod) {
    if (demod) {
        free(demod->i_samples);
        free(demod->q_samples);
        free(demod->demod_bits);
        free(demod);
    }
}
