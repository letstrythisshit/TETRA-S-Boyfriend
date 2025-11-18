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

    // Convert uint8 I/Q samples to float
    uint32_t sample_pairs = len / 2;
    if (sample_pairs > (uint32_t)demod->sample_count) {
        sample_pairs = demod->sample_count;
    }

    convert_uint8_to_float(iq_data, demod->i_samples, sample_pairs * 2);

    // Separate I and Q
    for (uint32_t i = 0; i < sample_pairs; i++) {
        demod->i_samples[i] = demod->i_samples[i * 2] - 127.5f;
        demod->q_samples[i] = demod->i_samples[i * 2 + 1] - 127.5f;
    }

    // Perform quadrature demodulation
    float *demod_output = malloc(sample_pairs * sizeof(float));
    if (!demod_output) return -1;

    quadrature_demod(demod->i_samples, demod->q_samples, demod_output, sample_pairs);

    // Apply low-pass filter
    low_pass_filter(demod_output, sample_pairs, 0.1f);

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

    // Search for training sequence in demodulated bits
    int best_match = 0;
    int best_offset = -1;

    for (int offset = 0; offset < demod->bit_count - 22; offset++) {
        int matches = 0;

        for (int i = 0; i < 22; i++) {
            if (demod->demod_bits[offset + i] == TETRA_TRAINING_SEQ[i]) {
                matches++;
            }
        }

        if (matches > best_match) {
            best_match = matches;
            best_offset = offset;
        }

        // Consider it a match if > 18 of 22 bits match (allows for some errors)
        if (matches >= 18) {
            log_message(true, "Training sequence found at offset %d (%d/22 matches)\n",
                       offset, matches);
            return true;
        }
    }

    // Weak detection for educational purposes
    if (best_match >= 15) {
        log_message(true, "Weak training sequence detected at offset %d (%d/22 matches)\n",
                   best_offset, best_match);
        return true;
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
