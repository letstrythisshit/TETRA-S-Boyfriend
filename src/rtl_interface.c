/*
 * RTL-SDR Interface Module
 * Handles RTL-SDR device initialization and I/Q sample capture
 */

#include "tetra_analyzer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// RTL-SDR library functions (will be linked if available)
#ifdef __has_include
#if __has_include(<rtl-sdr.h>)
#include <rtl-sdr.h>
#define HAS_RTLSDR 1
#endif
#endif

#ifndef HAS_RTLSDR
// Stub definitions if RTL-SDR library is not available
typedef void rtlsdr_dev_t;
static int rtlsdr_get_device_count(void) { return 0; }
static int rtlsdr_open(rtlsdr_dev_t **dev, uint32_t index) { (void)dev; (void)index; return -1; }
static int rtlsdr_close(rtlsdr_dev_t *dev) { (void)dev; return -1; }
static int rtlsdr_set_center_freq(rtlsdr_dev_t *dev, uint32_t freq) { (void)dev; (void)freq; return -1; }
static int rtlsdr_set_sample_rate(rtlsdr_dev_t *dev, uint32_t rate) { (void)dev; (void)rate; return -1; }
static int rtlsdr_set_tuner_gain_mode(rtlsdr_dev_t *dev, int manual) { (void)dev; (void)manual; return -1; }
static int rtlsdr_set_tuner_gain(rtlsdr_dev_t *dev, int gain) { (void)dev; (void)gain; return -1; }
static int rtlsdr_reset_buffer(rtlsdr_dev_t *dev) { (void)dev; return -1; }
static int rtlsdr_read_sync(rtlsdr_dev_t *dev, void *buf, int len, int *n_read) {
    (void)dev; (void)buf; (void)len; (void)n_read; return -1;
}
#endif

rtl_sdr_t* rtl_sdr_init(tetra_config_t *config) {
    rtl_sdr_t *sdr = calloc(1, sizeof(rtl_sdr_t));
    if (!sdr) {
        fprintf(stderr, "Failed to allocate SDR structure\n");
        return NULL;
    }

    int device_count = rtlsdr_get_device_count();
    if (device_count == 0) {
        fprintf(stderr, "No RTL-SDR devices found.\n");
        fprintf(stderr, "\nNOTE: This is a demonstration build.\n");
        fprintf(stderr, "To use with real hardware, install librtlsdr:\n");
        fprintf(stderr, "  Ubuntu/Debian: sudo apt-get install librtlsdr-dev\n");
        fprintf(stderr, "  Raspberry Pi: sudo apt-get install rtl-sdr librtlsdr-dev\n\n");
        fprintf(stderr, "Running in simulation mode for demonstration...\n\n");

        // Continue in simulation mode
        sdr->dev = NULL;
        sdr->frequency = config->frequency;
        sdr->sample_rate = config->sample_rate;
        sdr->gain = config->gain;
        sdr->running = false;
        return sdr;
    }

    log_message(true, "Found %d RTL-SDR device(s)\n", device_count);

    // Open device
    rtlsdr_dev_t *dev = NULL;
    if (rtlsdr_open(&dev, config->device_index) < 0) {
        fprintf(stderr, "Failed to open RTL-SDR device #%d\n", config->device_index);
        free(sdr);
        return NULL;
    }

    sdr->dev = dev;

    // Set frequency
    if (rtlsdr_set_center_freq(dev, config->frequency) < 0) {
        fprintf(stderr, "Failed to set center frequency\n");
        rtlsdr_close(dev);
        free(sdr);
        return NULL;
    }
    sdr->frequency = config->frequency;

    // Set sample rate
    if (rtlsdr_set_sample_rate(dev, config->sample_rate) < 0) {
        fprintf(stderr, "Failed to set sample rate\n");
        rtlsdr_close(dev);
        free(sdr);
        return NULL;
    }
    sdr->sample_rate = config->sample_rate;

    // Set gain
    if (config->auto_gain) {
        rtlsdr_set_tuner_gain_mode(dev, 0); // Auto gain
        log_message(config->verbose, "Using automatic gain\n");
    } else {
        rtlsdr_set_tuner_gain_mode(dev, 1); // Manual gain
        rtlsdr_set_tuner_gain(dev, config->gain * 10);
        log_message(config->verbose, "Set gain to %d dB\n", config->gain);
    }

    // Reset buffer
    rtlsdr_reset_buffer(dev);

    log_message(config->verbose, "RTL-SDR initialized successfully\n");

    return sdr;
}

int rtl_sdr_start(rtl_sdr_t *sdr, void (*callback)(uint8_t *buf, uint32_t len, void *ctx), void *ctx) {
    if (!sdr) return -1;

    sdr->running = true;

    // If no device (simulation mode), generate test data
    if (!sdr->dev) {
        log_message(true, "Running in SIMULATION mode - generating test TETRA signals\n");

        uint8_t *test_buffer = malloc(SDR_BUFFER_SIZE);
        if (!test_buffer) return -1;

        // Generate simulated I/Q data
        for (int iteration = 0; iteration < 100 && sdr->running; iteration++) {
            // Generate pseudo-random I/Q samples with TETRA-like characteristics
            for (int i = 0; i < SDR_BUFFER_SIZE; i++) {
                // Simple simulation: noise + carrier
                test_buffer[i] = 127 + (rand() % 50) - 25;
            }

            // Call callback with test data
            callback(test_buffer, SDR_BUFFER_SIZE, ctx);

            usleep(100000); // 100ms delay
        }

        free(test_buffer);
        return 0;
    }

    // Real RTL-SDR reading
    uint8_t *buffer = malloc(SDR_BUFFER_SIZE);
    if (!buffer) {
        fprintf(stderr, "Failed to allocate read buffer\n");
        return -1;
    }

    while (sdr->running) {
        int n_read = 0;
        int r = rtlsdr_read_sync(sdr->dev, buffer, SDR_BUFFER_SIZE, &n_read);

        if (r < 0) {
            fprintf(stderr, "RTL-SDR read error\n");
            break;
        }

        if (n_read > 0) {
            callback(buffer, n_read, ctx);
        }
    }

    free(buffer);
    return 0;
}

void rtl_sdr_stop(rtl_sdr_t *sdr) {
    if (sdr) {
        sdr->running = false;
    }
}

void rtl_sdr_cleanup(rtl_sdr_t *sdr) {
    if (sdr) {
        if (sdr->dev) {
            rtlsdr_close(sdr->dev);
        }
        free(sdr);
    }
}
