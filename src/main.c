/*
 * TETRA TEA1 Educational Security Research Toolkit
 *
 * Purpose: Educational demonstration of TETRA TEA1 encryption vulnerabilities
 * License: Educational and Research Use Only
 *
 * This software demonstrates the publicly disclosed vulnerabilities in TETRA TEA1
 * encryption as documented in academic security research (Midnight Blue, 2023).
 *
 * WARNING: This tool is for authorized security research and educational purposes only.
 * Only use on laboratory equipment and systems you own or have explicit permission to test.
 */

#include "tetra_analyzer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <getopt.h>

static volatile bool g_running = true;
static tetra_config_t g_config;
static rtl_sdr_t *g_sdr = NULL;
static tetra_demod_t *g_demod = NULL;
static tea1_context_t g_tea1_ctx;
static audio_output_t *g_audio = NULL;
static audio_playback_t *g_playback = NULL;
static tetra_codec_t *g_codec = NULL;
static detection_params_t *g_params = NULL;
static detection_status_t *g_status = NULL;

void signal_handler(int signum) {
    (void)signum;
    log_message(true, "\nShutting down gracefully...\n");
    g_running = false;
}

void print_banner(void) {
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║   TETRA TEA1 Educational Security Research Toolkit v%s   ║\n", TETRA_ANALYZER_VERSION);
    printf("║                                                               ║\n");
    printf("║   Educational demonstration of TETRA TEA1 vulnerabilities    ║\n");
    printf("║   Based on publicly disclosed security research              ║\n");
    printf("║                                                               ║\n");
    printf("║   ⚠️  FOR AUTHORIZED RESEARCH & EDUCATION ONLY ⚠️             ║\n");
    printf("║   Use only on laboratory equipment you own/control           ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n");
    printf("\n");
}

void print_usage(const char *prog) {
    printf("Usage: %s [OPTIONS]\n\n", prog);
    printf("Options:\n");
    printf("  -f, --frequency FREQ   Frequency in Hz (default: 420000000)\n");
    printf("  -s, --sample-rate SR   Sample rate (default: 2400000)\n");
    printf("  -g, --gain GAIN        Tuner gain in dB (default: auto)\n");
    printf("  -d, --device INDEX     RTL-SDR device index (default: 0)\n");
    printf("  -o, --output FILE      Output audio file (WAV format)\n");
    printf("  -r, --realtime-audio   Enable real-time audio playback 🔊\n");
    printf("  -G, --gui              Enable GTK+ graphical interface 🖥️\n");
    printf("  -v, --verbose          Verbose output\n");
    printf("  -k, --use-vulnerability Use known TEA1 vulnerability\n");
    printf("  -h, --help             Show this help\n\n");
    printf("Examples:\n");
    printf("  %s -f 420000000 -v              # Listen on 420 MHz\n", prog);
    printf("  %s -f 420000000 -k -r -v        # Decrypt and hear audio in real-time\n", prog);
    printf("  %s -f 420000000 -k -o audio.wav # Crack and save audio\n", prog);
    printf("  %s -f 420000000 -G              # Launch with graphical interface\n", prog);
    printf("\n");
}

void sdr_callback(uint8_t *buf, uint32_t len, void *ctx) {
    (void)ctx;

    if (!g_running) return;

    // Process samples through TETRA demodulator
    if (tetra_demod_process(g_demod, buf, len) > 0) {
        // Check if we detected a TETRA burst
        if (tetra_detect_burst(g_demod)) {
            log_message(g_config.verbose, "TETRA burst detected!\n");

            // Attempt TEA1 decryption if we have encrypted data
            if (g_config.use_known_vulnerability && g_demod->bit_count >= TETRA_CODEC_FRAME_SIZE) {
                uint8_t encrypted_bits[TETRA_CODEC_FRAME_SIZE / 8 + 1];
                uint8_t decrypted_bits[TETRA_CODEC_FRAME_SIZE / 8 + 1];

                // Convert demodulated bits to bytes
                int byte_count = (TETRA_CODEC_FRAME_SIZE + 7) / 8;
                for (int i = 0; i < byte_count; i++) {
                    encrypted_bits[i] = 0;
                    for (int j = 0; j < 8 && (i * 8 + j) < TETRA_CODEC_FRAME_SIZE; j++) {
                        if (i * 8 + j < g_demod->bit_count) {
                            encrypted_bits[i] |= (g_demod->demod_bits[i * 8 + j] << (7 - j));
                        }
                    }
                }

                // Decrypt the frame (simplified - treating as stream)
                tea1_decrypt_stream(&g_tea1_ctx, encrypted_bits, decrypted_bits, byte_count);

                if (g_config.verbose && g_demod->bit_count >= TEA1_BLOCK_SIZE * 8) {
                    hex_dump(encrypted_bits, TEA1_BLOCK_SIZE, "Encrypted");
                    hex_dump(decrypted_bits, TEA1_BLOCK_SIZE, "Decrypted");
                }

                // Decode TETRA audio codec if we have enough data
                if (g_codec && g_demod->bit_count >= TETRA_CODEC_FRAME_SIZE) {
                    int16_t audio_samples[TETRA_CODEC_SAMPLES];

                    // Decode the audio frame
                    int decoded = tetra_codec_decode_frame(g_codec, decrypted_bits, audio_samples);

                    if (decoded > 0) {
                        // Send to real-time playback if enabled
                        if (g_playback && g_config.enable_realtime_audio) {
                            audio_playback_write(g_playback, audio_samples, decoded);
                        }

                        // Also write to file if specified
                        if (g_audio) {
                            audio_output_write(g_audio, audio_samples, decoded);
                        }
                    }
                }
            }
        }
    }
}

int main(int argc, char **argv) {
    // Initialize default configuration
    memset(&g_config, 0, sizeof(g_config));
    g_config.frequency = 420000000;  // 420 MHz
    g_config.sample_rate = TETRA_SAMPLE_RATE;
    g_config.gain = 0;
    g_config.auto_gain = true;
    g_config.device_index = 0;
    g_config.verbose = false;
    g_config.use_known_vulnerability = false;
    g_config.enable_realtime_audio = false;
    g_config.enable_gui = false;
    g_config.output_file = NULL;

    // Parse command line arguments
    static struct option long_options[] = {
        {"frequency", required_argument, 0, 'f'},
        {"sample-rate", required_argument, 0, 's'},
        {"gain", required_argument, 0, 'g'},
        {"device", required_argument, 0, 'd'},
        {"output", required_argument, 0, 'o'},
        {"realtime-audio", no_argument, 0, 'r'},
        {"gui", no_argument, 0, 'G'},
        {"verbose", no_argument, 0, 'v'},
        {"use-vulnerability", no_argument, 0, 'k'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "f:s:g:d:o:rGvkh", long_options, NULL)) != -1) {
        switch (opt) {
            case 'f':
                g_config.frequency = atoi(optarg);
                break;
            case 's':
                g_config.sample_rate = atoi(optarg);
                break;
            case 'g':
                g_config.gain = atoi(optarg);
                g_config.auto_gain = false;
                break;
            case 'd':
                g_config.device_index = atoi(optarg);
                break;
            case 'o':
                g_config.output_file = optarg;
                break;
            case 'r':
                g_config.enable_realtime_audio = true;
                break;
            case 'G':
#ifdef HAVE_GTK
                g_config.enable_gui = true;
#else
                fprintf(stderr, "GUI support not compiled in. Rebuild with GTK3 installed.\n");
                return 1;
#endif
                break;
            case 'v':
                g_config.verbose = true;
                break;
            case 'k':
                g_config.use_known_vulnerability = true;
                break;
            case 'h':
                print_banner();
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }

    print_banner();

    // Validate frequency range
    if (g_config.frequency < TETRA_FREQUENCY_MIN ||
        g_config.frequency > TETRA_FREQUENCY_MAX) {
        fprintf(stderr, "Error: Frequency must be between %d and %d Hz\n",
                TETRA_FREQUENCY_MIN, TETRA_FREQUENCY_MAX);
        return 1;
    }

    // Setup signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Initialize components
    log_message(true, "Initializing TETRA analyzer...\n");
    log_message(true, "Frequency: %u Hz (%.3f MHz)\n",
                g_config.frequency, g_config.frequency / 1e6);
    log_message(true, "Sample Rate: %u Hz\n", g_config.sample_rate);

    if (g_config.use_known_vulnerability) {
        log_message(true, "⚠️  TEA1 vulnerability exploitation ENABLED\n");
        log_message(true, "Using reduced 32-bit keyspace attack\n");
    }

    // Initialize detection parameters and status
    g_params = detection_params_init();
    if (!g_params) {
        fprintf(stderr, "Failed to initialize detection parameters\n");
        return 1;
    }

    g_status = detection_status_init();
    if (!g_status) {
        fprintf(stderr, "Failed to initialize detection status\n");
        detection_params_cleanup(g_params);
        return 1;
    }

    // Initialize RTL-SDR
    g_sdr = rtl_sdr_init(&g_config);
    if (!g_sdr) {
        fprintf(stderr, "Failed to initialize RTL-SDR\n");
        detection_status_cleanup(g_status);
        detection_params_cleanup(g_params);
        return 1;
    }

    // Initialize TETRA demodulator
    g_demod = tetra_demod_init(g_config.sample_rate, g_params, g_status);
    if (!g_demod) {
        fprintf(stderr, "Failed to initialize TETRA demodulator\n");
        rtl_sdr_cleanup(g_sdr);
        detection_status_cleanup(g_status);
        detection_params_cleanup(g_params);
        return 1;
    }

    // Initialize TEA1 context
    uint8_t default_key[TEA1_KEY_SIZE] = {0}; // Will be cracked
    tea1_init(&g_tea1_ctx, default_key, g_config.use_known_vulnerability);

    // Initialize TETRA codec (needed for real-time audio or file output)
    if (g_config.enable_realtime_audio || g_config.output_file) {
        g_codec = tetra_codec_init();
        if (!g_codec) {
            fprintf(stderr, "Warning: Failed to initialize TETRA codec\n");
        }
    }

    // Initialize real-time audio playback if enabled
    if (g_config.enable_realtime_audio) {
        g_playback = audio_playback_init(TETRA_AUDIO_SAMPLE_RATE);
        if (g_playback) {
            audio_playback_start(g_playback);
            log_message(true, "\n");
        } else {
            fprintf(stderr, "Warning: Failed to initialize real-time audio playback\n");
            log_message(true, "Continuing without real-time audio...\n");
        }
    }

    // Initialize audio output if specified
    if (g_config.output_file) {
        g_audio = audio_output_init(g_config.output_file, TETRA_AUDIO_SAMPLE_RATE);
        if (!g_audio) {
            fprintf(stderr, "Warning: Failed to initialize audio output\n");
        }
    }

    // Start SDR capture
    log_message(true, "Starting SDR capture...\n");
    if (g_config.enable_realtime_audio) {
        log_message(true, "🎧 Real-time audio enabled - listen to your speakers!\n");
    }
    if (!g_config.enable_gui) {
        log_message(true, "Press Ctrl+C to stop\n\n");
    }

    if (rtl_sdr_start(g_sdr, sdr_callback, NULL) < 0) {
        fprintf(stderr, "Failed to start SDR capture\n");
        tetra_demod_cleanup(g_demod);
        rtl_sdr_cleanup(g_sdr);
        detection_status_cleanup(g_status);
        detection_params_cleanup(g_params);
        return 1;
    }

#ifdef HAVE_GTK
    // Launch GUI if enabled
    if (g_config.enable_gui) {
        log_message(true, "Launching graphical interface...\n\n");

        tetra_gui_t *gui = tetra_gui_init(&g_config, g_params, g_status, g_sdr);
        if (!gui) {
            fprintf(stderr, "Failed to initialize GUI\n");
            rtl_sdr_stop(g_sdr);
            rtl_sdr_cleanup(g_sdr);
            tetra_demod_cleanup(g_demod);
            detection_status_cleanup(g_status);
            detection_params_cleanup(g_params);
            return 1;
        }

        // Run GUI main loop (blocks until window is closed)
        tetra_gui_run(gui);

        // Cleanup GUI
        tetra_gui_cleanup(gui);
        g_running = false;
    } else {
#endif
        // Traditional CLI main loop
        while (g_running) {
            usleep(100000); // 100ms
        }
#ifdef HAVE_GTK
    }
#endif

    // Cleanup
    log_message(true, "\nCleaning up...\n");
    rtl_sdr_stop(g_sdr);
    rtl_sdr_cleanup(g_sdr);
    tetra_demod_cleanup(g_demod);

    if (g_playback) {
        audio_playback_cleanup(g_playback);
    }

    if (g_audio) {
        audio_output_cleanup(g_audio);
    }

    if (g_codec) {
        tetra_codec_cleanup(g_codec);
    }

    if (g_status) {
        detection_status_cleanup(g_status);
    }

    if (g_params) {
        detection_params_cleanup(g_params);
    }

    log_message(true, "Shutdown complete.\n");

    return 0;
}
