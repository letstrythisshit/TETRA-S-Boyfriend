/*
 * TEA1 Cryptography Module
 * Implementation of TETRA TEA1 encryption/decryption
 *
 * Note: This implements the documented vulnerabilities in TEA1:
 * - Reduced effective key size (32 bits instead of 80 bits)
 * - Weak key schedule allowing key recovery
 *
 * Reference: "TETRA:BURST" - Midnight Blue Security Research (2023)
 */

#include "tetra_analyzer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// TEA1 internal state (simplified educational implementation)
typedef struct {
    uint32_t state[4];
    uint32_t round_keys[32];
} tea1_internal_t;

// TEA1 S-box (simplified for demonstration)
static const uint8_t TEA1_SBOX[256] = {
    0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
    0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
    // ... (AES S-box used as placeholder for educational purposes)
    0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
    0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
    0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
    0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
    0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
    0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
    0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
    0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
    0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
    0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
    0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
    0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
    0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
    0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16
};

// Extract reduced 32-bit key from full 80-bit key (demonstrating the vulnerability)
uint32_t tea1_extract_reduced_key(const uint8_t *full_key) {
    // The TEA1 vulnerability: due to weak key schedule, only 32 bits are effective
    // This is a simplification of the actual vulnerability documented in research
    uint32_t reduced = 0;

    // Extract specific bits that actually affect encryption (vulnerability)
    reduced |= ((uint32_t)full_key[0] & 0xFF) << 24;
    reduced |= ((uint32_t)full_key[1] & 0xFF) << 16;
    reduced |= ((uint32_t)full_key[2] & 0xFF) << 8;
    reduced |= ((uint32_t)full_key[3] & 0xFF);

    // Remaining 48 bits (full_key[4-9]) are effectively ignored due to weak key schedule
    // This is the core vulnerability that reduces keyspace from 2^80 to 2^32

    return reduced;
}

// TEA1 key schedule (with vulnerability)
static void tea1_key_schedule(tea1_context_t *ctx, tea1_internal_t *internal) {
    if (ctx->vulnerability_mode) {
        // Exploit the vulnerability: use only the reduced 32-bit key
        uint32_t rk = ctx->reduced_key;

        for (int i = 0; i < 32; i++) {
            // Weak key schedule that repeats pattern
            internal->round_keys[i] = rk ^ (i * 0x9E3779B9); // TEA constant
            rk = (rk << 1) | (rk >> 31); // Rotate
        }
    } else {
        // Full 80-bit key schedule (still weak but uses more bits)
        uint32_t k0 = ((uint32_t)ctx->key[0] << 24) | ((uint32_t)ctx->key[1] << 16) |
                      ((uint32_t)ctx->key[2] << 8) | ctx->key[3];
        uint32_t k1 = ((uint32_t)ctx->key[4] << 24) | ((uint32_t)ctx->key[5] << 16) |
                      ((uint32_t)ctx->key[6] << 8) | ctx->key[7];
        uint32_t k2 = ((uint32_t)ctx->key[8] << 8) | ctx->key[9];

        for (int i = 0; i < 32; i++) {
            internal->round_keys[i] = k0 ^ k1 ^ k2 ^ (i * 0x9E3779B9);
            k0 = (k0 << 1) | (k0 >> 31);
            k1 = (k1 >> 1) | (k1 << 31);
        }
    }
}

// TEA1 round function (simplified)
static uint32_t tea1_round(uint32_t data, uint32_t round_key) {
    // Apply S-box
    uint8_t *bytes = (uint8_t *)&data;
    for (int i = 0; i < 4; i++) {
        bytes[i] = TEA1_SBOX[bytes[i]];
    }

    // XOR with round key
    data ^= round_key;

    // Simple permutation
    data = (data << 7) | (data >> 25);

    return data;
}

void tea1_init(tea1_context_t *ctx, const uint8_t *key, bool use_vulnerability) {
    memcpy(ctx->key, key, TEA1_KEY_SIZE);
    memset(ctx->iv, 0, TEA1_BLOCK_SIZE);
    ctx->vulnerability_mode = use_vulnerability;

    if (use_vulnerability) {
        ctx->reduced_key = tea1_extract_reduced_key(key);
        log_message(true, "TEA1: Using vulnerability mode (32-bit keyspace)\n");
        log_message(true, "TEA1: Reduced key = 0x%08X\n", ctx->reduced_key);
    }
}

void tea1_decrypt_block(tea1_context_t *ctx, const uint8_t *input, uint8_t *output) {
    tea1_internal_t internal;

    // Load input block
    internal.state[0] = ((uint32_t)input[0] << 24) | ((uint32_t)input[1] << 16) |
                        ((uint32_t)input[2] << 8) | input[3];
    internal.state[1] = ((uint32_t)input[4] << 24) | ((uint32_t)input[5] << 16) |
                        ((uint32_t)input[6] << 8) | input[7];

    // Generate round keys
    tea1_key_schedule(ctx, &internal);

    // Decrypt (reverse rounds)
    for (int round = 31; round >= 0; round--) {
        uint32_t temp = internal.state[1];
        internal.state[1] = internal.state[0];
        internal.state[0] = tea1_round(temp, internal.round_keys[round]);
    }

    // Store output block
    output[0] = (internal.state[0] >> 24) & 0xFF;
    output[1] = (internal.state[0] >> 16) & 0xFF;
    output[2] = (internal.state[0] >> 8) & 0xFF;
    output[3] = internal.state[0] & 0xFF;
    output[4] = (internal.state[1] >> 24) & 0xFF;
    output[5] = (internal.state[1] >> 16) & 0xFF;
    output[6] = (internal.state[1] >> 8) & 0xFF;
    output[7] = internal.state[1] & 0xFF;
}

void tea1_decrypt_stream(tea1_context_t *ctx, const uint8_t *input, uint8_t *output, size_t len) {
    size_t blocks = len / TEA1_BLOCK_SIZE;

    for (size_t i = 0; i < blocks; i++) {
        tea1_decrypt_block(ctx, input + (i * TEA1_BLOCK_SIZE), output + (i * TEA1_BLOCK_SIZE));

        // XOR with IV for CBC mode (simplified)
        for (int j = 0; j < TEA1_BLOCK_SIZE; j++) {
            output[i * TEA1_BLOCK_SIZE + j] ^= ctx->iv[j];
        }

        // Update IV
        memcpy(ctx->iv, input + (i * TEA1_BLOCK_SIZE), TEA1_BLOCK_SIZE);
    }
}
