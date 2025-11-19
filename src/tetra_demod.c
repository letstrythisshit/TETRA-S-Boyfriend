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

tetra_demod_t* tetra_demod_init(uint32_t sample_rate, detection_params_t *params, detection_status_t *status) {
    (void)sample_rate; // Reserved for future use

    tetra_demod_t *demod = calloc(1, sizeof(tetra_demod_t));
    if (!demod) {
        fprintf(stderr, "Failed to allocate demodulator structure\n");
        return NULL;
    }

    demod->sample_count = SDR_BUFFER_SIZE / 2; // I/Q pairs
    demod->squelch_threshold = squelch_threshold;

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
    demod->params = params;
    demod->status = status;

    log_message(true, "TETRA demodulator: squelch = %.1f (adjust with -q if needed)\n", squelch_threshold);

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

    // STEP 1: Check signal strength (squelch)
    float signal_power = detect_signal_strength(demod->i_samples, demod->q_samples, sample_pairs);

    // Require minimum signal strength (user-adjustable threshold)
    // Typical TETRA signal: 20-50, noise: <10
    if (signal_power < demod->squelch_threshold) {
        demod->bit_count = 0;
        return 0;  // Too weak, probably just noise
    }

    // Perform quadrature demodulation
    float *demod_output = malloc(sample_pairs * sizeof(float));
    if (!demod_output) return -1;

    quadrature_demod(demod->i_samples, demod->q_samples, demod_output, sample_pairs);

    // Apply low-pass filter (using dynamic parameter from GUI)
    float lpf_cutoff = 0.5f; // Default value
    if (demod->params) {
        pthread_mutex_lock(&demod->params->lock);
        lpf_cutoff = demod->params->lpf_cutoff;
        pthread_mutex_unlock(&demod->params->lock);
    }
    low_pass_filter(demod_output, sample_pairs, lpf_cutoff);

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

    // Load dynamic parameters (thread-safe)
    float min_signal_power = 8.0f;
    int strong_match_threshold = 20;
    int moderate_match_threshold = 19;
    float strong_correlation = 0.8f;
    float moderate_correlation = 0.75f;
    float moderate_power_multiplier = 1.2f;

    if (demod->params) {
        pthread_mutex_lock(&demod->params->lock);
        min_signal_power = demod->params->min_signal_power;
        strong_match_threshold = demod->params->strong_match_threshold;
        moderate_match_threshold = demod->params->moderate_match_threshold;
        strong_correlation = demod->params->strong_correlation;
        moderate_correlation = demod->params->moderate_correlation;
        moderate_power_multiplier = demod->params->moderate_power_multiplier;
        pthread_mutex_unlock(&demod->params->lock);
    }

    // Step 1: Check signal power to reject pure noise
    // Calculate RMS power from I/Q samples
    float signal_power = detect_signal_strength(demod->i_samples, demod->q_samples,
                                                 demod->sample_count);

    // Update status with current signal power
    if (demod->status) {
        pthread_mutex_lock(&demod->status->lock);
        demod->status->current_signal_power = signal_power;
        pthread_mutex_unlock(&demod->status->lock);
    }

    if (signal_power < min_signal_power) {
        log_message(true, "Signal power too low: %.2f < %.2f (rejecting noise)\n",
                   signal_power, min_signal_power);
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

        // Strong match threshold (configurable via GUI)
        if (matches >= strong_match_threshold && correlation >= strong_correlation) {
            log_message(true, "TETRA burst detected at offset %d (%d/22 matches, corr=%.3f, power=%.2f)\n",
                       offset, matches, correlation, signal_power);

            // Update detection status
            if (demod->status) {
                pthread_mutex_lock(&demod->status->lock);
                demod->status->burst_detected = true;
                demod->status->last_match_count = matches;
                demod->status->last_correlation = correlation;
                demod->status->last_offset = offset;
                demod->status->last_detection_time = get_timestamp_us();
                demod->status->detection_count++;
                pthread_mutex_unlock(&demod->status->lock);
            }

            return true;
        }
    }

    // Moderate detection threshold (configurable via GUI)
    if (best_match >= moderate_match_threshold &&
        best_correlation >= moderate_correlation &&
        signal_power >= min_signal_power * moderate_power_multiplier) {
        log_message(true, "TETRA burst detected (moderate) at offset %d (%d/22 matches, corr=%.3f, power=%.2f)\n",
                   best_offset, best_match, best_correlation, signal_power);

        // Update detection status
        if (demod->status) {
            pthread_mutex_lock(&demod->status->lock);
            demod->status->burst_detected = true;
            demod->status->last_match_count = best_match;
            demod->status->last_correlation = best_correlation;
            demod->status->last_offset = best_offset;
            demod->status->last_detection_time = get_timestamp_us();
            demod->status->detection_count++;
            pthread_mutex_unlock(&demod->status->lock);
        }

        return true;
    }

    // Update status for rejection
    if (demod->status) {
        pthread_mutex_lock(&demod->status->lock);
        demod->status->burst_detected = false;
        demod->status->last_match_count = best_match;
        demod->status->last_correlation = best_correlation;
        demod->status->last_offset = best_offset;
        pthread_mutex_unlock(&demod->status->lock);
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

// Detection parameters management

detection_params_t* detection_params_init(void) {
    detection_params_t *params = calloc(1, sizeof(detection_params_t));
    if (!params) {
        fprintf(stderr, "Failed to allocate detection parameters\n");
        return NULL;
    }

    pthread_mutex_init(&params->lock, NULL);
    detection_params_reset_defaults(params);

    return params;
}

void detection_params_cleanup(detection_params_t *params) {
    if (params) {
        pthread_mutex_destroy(&params->lock);
        free(params);
    }
}

void detection_params_reset_defaults(detection_params_t *params) {
    if (!params) return;

    pthread_mutex_lock(&params->lock);
    params->min_signal_power = 8.0f;
    params->strong_match_threshold = 20;
    params->moderate_match_threshold = 19;
    params->strong_correlation = 0.8f;
    params->moderate_correlation = 0.75f;
    params->lpf_cutoff = 0.5f;
    params->moderate_power_multiplier = 1.2f;
    pthread_mutex_unlock(&params->lock);
}

// Detection status management

detection_status_t* detection_status_init(void) {
    detection_status_t *status = calloc(1, sizeof(detection_status_t));
    if (!status) {
        fprintf(stderr, "Failed to allocate detection status\n");
        return NULL;
    }

    pthread_mutex_init(&status->lock, NULL);
    detection_status_reset(status);

    return status;
}

void detection_status_cleanup(detection_status_t *status) {
    if (status) {
        pthread_mutex_destroy(&status->lock);
        free(status);
    }
}

void detection_status_reset(detection_status_t *status) {
    if (!status) return;

    pthread_mutex_lock(&status->lock);
    status->current_signal_power = 0.0f;
    status->last_match_count = 0;
    status->last_correlation = 0.0f;
    status->last_offset = -1;
    status->burst_detected = false;
    status->last_detection_time = 0;
    status->detection_count = 0;
    pthread_mutex_unlock(&status->lock);
}
