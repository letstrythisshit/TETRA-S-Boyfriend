/*
 * Utility Functions
 * Helper functions for logging, debugging, and timing
 */

#include "tetra_analyzer.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <sys/time.h>

void hex_dump(const uint8_t *data, size_t len, const char *label) {
    if (label) {
        printf("%s (%zu bytes):\n", label, len);
    }

    for (size_t i = 0; i < len; i++) {
        printf("%02X ", data[i]);

        if ((i + 1) % 16 == 0) {
            printf("\n");
        } else if ((i + 1) % 8 == 0) {
            printf(" ");
        }
    }

    if (len % 16 != 0) {
        printf("\n");
    }
}

uint64_t get_timestamp_us(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000 + tv.tv_usec;
}

void log_message(bool verbose, const char *format, ...) {
    if (!verbose) return;

    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    fflush(stdout);
}

// Binary to string conversion
void bits_to_string(const uint8_t *bits, size_t len, char *output) {
    for (size_t i = 0; i < len; i++) {
        output[i] = bits[i] ? '1' : '0';
    }
    output[len] = '\0';
}

// Calculate bit error rate
float calculate_ber(const uint8_t *received, const uint8_t *expected, size_t len) {
    size_t errors = 0;

    for (size_t i = 0; i < len; i++) {
        if (received[i] != expected[i]) {
            errors++;
        }
    }

    return (float)errors / len;
}
