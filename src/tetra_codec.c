/*
 * TETRA Audio Codec Module
 * Simplified ACELP-based codec decoder for TETRA voice
 *
 * TETRA uses ACELP (Algebraic Code Excited Linear Prediction) codec
 * This is a simplified educational implementation for demonstration
 */

#include "tetra_analyzer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// LPC (Linear Predictive Coding) order
#define LPC_ORDER 10

// Extract bits from byte array
static uint32_t extract_bits(const uint8_t *data, int start_bit, int num_bits) {
    uint32_t result = 0;

    for (int i = 0; i < num_bits; i++) {
        int byte_idx = (start_bit + i) / 8;
        int bit_idx = 7 - ((start_bit + i) % 8);

        if (data[byte_idx] & (1 << bit_idx)) {
            result |= (1 << (num_bits - 1 - i));
        }
    }

    return result;
}

// Decode LPC coefficients from quantized values
static void decode_lpc_coeffs(uint32_t lpc_params, float *coeffs) {
    // Simplified LPC decoding
    // Real TETRA uses LSF (Line Spectral Frequencies) quantization

    for (int i = 0; i < LPC_ORDER; i++) {
        // Extract bits for this coefficient
        int bits = (lpc_params >> (i * 3)) & 0x7;

        // Convert to coefficient (-0.9 to 0.9 range)
        coeffs[i] = ((float)bits - 3.5f) / 4.0f;
    }
}

// Generate excitation signal from codebook
static void generate_excitation(tetra_codec_t *codec, uint32_t codebook_idx,
                                float gain, float *excitation) {
    // Simplified ACELP codebook
    // Real TETRA uses algebraic codebook + adaptive codebook

    // Clear excitation
    memset(excitation, 0, TETRA_CODEC_SAMPLES * sizeof(float));

    // Generate pulse positions from codebook index
    for (int i = 0; i < 4; i++) {
        int pulse_pos = ((codebook_idx >> (i * 6)) & 0x3F) % TETRA_CODEC_SAMPLES;
        float pulse_sign = ((codebook_idx >> (i * 6 + 6)) & 1) ? 1.0f : -1.0f;

        excitation[pulse_pos] += pulse_sign * gain;
    }
}

// LPC synthesis filter
static void lpc_synthesis(const float *lpc_coeffs, const float *excitation,
                         float *output, int length) {
    float state[LPC_ORDER] = {0};

    for (int n = 0; n < length; n++) {
        float prediction = 0.0f;

        // LPC prediction
        for (int k = 0; k < LPC_ORDER; k++) {
            if (n - k - 1 >= 0) {
                prediction += lpc_coeffs[k] * output[n - k - 1];
            } else {
                prediction += lpc_coeffs[k] * state[LPC_ORDER - 1 + (n - k - 1)];
            }
        }

        // Add excitation
        output[n] = excitation[n] + prediction;

        // Apply soft limiting
        if (output[n] > 1.0f) output[n] = 1.0f;
        if (output[n] < -1.0f) output[n] = -1.0f;
    }
}

// Post-processing and de-emphasis filter
static void post_process(float *samples, int length) {
    // De-emphasis filter (inverse pre-emphasis)
    float alpha = 0.95f;

    for (int i = length - 1; i > 0; i--) {
        samples[i] = samples[i] + alpha * samples[i - 1];
    }
}

tetra_codec_t* tetra_codec_init(void) {
    tetra_codec_t *codec = calloc(1, sizeof(tetra_codec_t));
    if (!codec) {
        fprintf(stderr, "Failed to allocate TETRA codec structure\n");
        return NULL;
    }

    codec->pitch_period = 40.0f;  // ~200 Hz typical pitch
    codec->pitch_gain = 0.5f;
    codec->frame_count = 0;

    // Initialize LPC coefficients to neutral
    for (int i = 0; i < LPC_ORDER; i++) {
        codec->lpc_coeffs[i] = 0.0f;
    }

    log_message(true, "TETRA codec initialized (ACELP-based, 8 kHz)\n");

    return codec;
}

int tetra_codec_decode_frame(tetra_codec_t *codec, const uint8_t *encoded_bits,
                             int16_t *audio_samples) {
    if (!codec || !encoded_bits || !audio_samples) {
        return -1;
    }

    // TETRA codec frame structure (simplified):
    // - LPC parameters: 30 bits
    // - Pitch period: 7 bits
    // - Pitch gain: 4 bits
    // - Fixed codebook: 4 pulses × 13 bits = 52 bits
    // - Gains: 20 bits
    // Total: ~137 bits per frame

    // Extract parameters from bitstream
    uint32_t lpc_params = extract_bits(encoded_bits, 0, 30);
    uint32_t pitch_period_idx = extract_bits(encoded_bits, 30, 7);
    uint32_t pitch_gain_idx = extract_bits(encoded_bits, 37, 4);
    uint32_t codebook_idx = extract_bits(encoded_bits, 41, 52);
    uint32_t fixed_gain_idx = extract_bits(encoded_bits, 93, 10);

    // Decode LPC coefficients
    decode_lpc_coeffs(lpc_params, codec->lpc_coeffs);

    // Decode pitch parameters
    codec->pitch_period = 20.0f + (float)pitch_period_idx * 0.5f;  // 20-83.5 samples
    codec->pitch_gain = (float)pitch_gain_idx / 15.0f;  // 0.0 to 1.0

    // Decode gain
    float fixed_gain = powf(10.0f, ((float)fixed_gain_idx - 512.0f) / 20.0f / 20.0f);

    // Generate excitation signal
    float excitation[TETRA_CODEC_SAMPLES];
    generate_excitation(codec, codebook_idx, fixed_gain, excitation);

    // Add pitch prediction (adaptive codebook)
    for (int i = 0; i < TETRA_CODEC_SAMPLES; i++) {
        int pitch_idx = i - (int)codec->pitch_period;
        if (pitch_idx >= 0) {
            excitation[i] += codec->pitch_gain * excitation[pitch_idx];
        } else if (pitch_idx >= -TETRA_CODEC_SAMPLES) {
            excitation[i] += codec->pitch_gain * codec->excitation[TETRA_CODEC_SAMPLES + pitch_idx];
        }
    }

    // Save excitation for next frame
    memcpy(codec->excitation, excitation, TETRA_CODEC_SAMPLES * sizeof(float));

    // LPC synthesis filtering
    float decoded_samples[TETRA_CODEC_SAMPLES];
    lpc_synthesis(codec->lpc_coeffs, excitation, decoded_samples, TETRA_CODEC_SAMPLES);

    // Post-processing
    post_process(decoded_samples, TETRA_CODEC_SAMPLES);

    // Convert to int16 samples
    for (int i = 0; i < TETRA_CODEC_SAMPLES; i++) {
        // Scale to int16 range with soft clipping
        float sample = decoded_samples[i] * 16384.0f;  // Leave headroom

        if (sample > 32767.0f) sample = 32767.0f;
        if (sample < -32768.0f) sample = -32768.0f;

        audio_samples[i] = (int16_t)sample;
    }

    codec->frame_count++;

    return TETRA_CODEC_SAMPLES;
}

void tetra_codec_cleanup(tetra_codec_t *codec) {
    if (codec) {
        log_message(true, "TETRA codec decoded %d frames\n", codec->frame_count);
        free(codec);
    }
}
