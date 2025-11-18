#ifndef TETRA_ANALYZER_H
#define TETRA_ANALYZER_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <pthread.h>

// Version information
#define TETRA_ANALYZER_VERSION "1.0.0-educational"

// TETRA constants
#define TETRA_FREQUENCY_MIN 380000000  // 380 MHz
#define TETRA_FREQUENCY_MAX 470000000  // 470 MHz
#define TETRA_SAMPLE_RATE 2400000      // 2.4 MHz (optimized for low resources)
#define TETRA_SYMBOL_RATE 18000        // 18 kHz
#define TETRA_BURST_LENGTH 510         // symbols per burst

// TEA1 constants
#define TEA1_KEY_SIZE 10               // 80 bits = 10 bytes
#define TEA1_EFFECTIVE_KEY_SIZE 4      // 32 bits due to known vulnerability
#define TEA1_BLOCK_SIZE 8              // 64 bits = 8 bytes

// Buffer sizes (optimized for low memory)
#define SDR_BUFFER_SIZE (16 * 16384)   // 256KB
#define AUDIO_BUFFER_SIZE 8192
#define AUDIO_RING_BUFFER_SIZE (8192 * 4)  // Ring buffer for smooth playback
#define MAX_CHANNELS 4

// TETRA audio codec constants
#define TETRA_CODEC_FRAME_SIZE 137     // bits per codec frame
#define TETRA_CODEC_SAMPLES 160        // samples per frame (20ms @ 8kHz)
#define TETRA_AUDIO_SAMPLE_RATE 8000   // 8 kHz audio

// Configuration structure
typedef struct {
    uint32_t frequency;
    uint32_t sample_rate;
    int gain;
    bool auto_gain;
    bool verbose;
    bool use_known_vulnerability;
    bool enable_realtime_audio;
    bool enable_gui;
    char *output_file;
    int device_index;
} tetra_config_t;

// RTL-SDR interface
typedef struct {
    void *dev;
    uint32_t frequency;
    uint32_t sample_rate;
    int gain;
    bool running;
    pthread_t thread;
} rtl_sdr_t;

// Dynamic detection parameters (configurable via GUI)
typedef struct {
    float min_signal_power;          // Minimum signal power threshold (default: 8.0)
    int strong_match_threshold;      // Strong match bits required (default: 20/22)
    int moderate_match_threshold;    // Moderate match bits required (default: 19/22)
    float strong_correlation;        // Strong correlation threshold (default: 0.8)
    float moderate_correlation;      // Moderate correlation threshold (default: 0.75)
    float lpf_cutoff;                // Low-pass filter cutoff (default: 0.5)
    float moderate_power_multiplier; // Power multiplier for moderate detection (default: 1.2)
    pthread_mutex_t lock;            // Mutex for thread-safe parameter updates
} detection_params_t;

// Real-time status information
typedef struct {
    float current_signal_power;
    int last_match_count;
    float last_correlation;
    int last_offset;
    bool burst_detected;
    uint64_t last_detection_time;
    uint64_t detection_count;
    pthread_mutex_t lock;            // Mutex for thread-safe status updates
} detection_status_t;

// TETRA demodulator state
typedef struct {
    uint32_t frequency;
    float *i_samples;
    float *q_samples;
    int sample_count;
    float symbol_timing;
    uint8_t *demod_bits;
    int bit_count;
    detection_params_t *params;      // Pointer to shared detection parameters
    detection_status_t *status;      // Pointer to shared status information
} tetra_demod_t;

// TEA1 encryption context
typedef struct {
    uint8_t key[TEA1_KEY_SIZE];
    uint8_t iv[TEA1_BLOCK_SIZE];
    uint32_t reduced_key;  // For vulnerability exploitation
    bool vulnerability_mode;
} tea1_context_t;

// Audio output
typedef struct {
    int16_t *buffer;
    int buffer_size;
    int sample_rate;
    FILE *output_file;
} audio_output_t;

// Real-time audio playback (ALSA)
typedef struct {
    void *pcm_handle;
    int16_t *ring_buffer;
    int ring_write_pos;
    int ring_read_pos;
    int ring_size;
    int sample_rate;
    bool running;
    pthread_t playback_thread;
    pthread_mutex_t buffer_mutex;
} audio_playback_t;

// TETRA codec state
typedef struct {
    float prev_samples[TETRA_CODEC_SAMPLES];
    float excitation[TETRA_CODEC_SAMPLES];
    float lpc_coeffs[10];
    float pitch_gain;
    float pitch_period;
    int frame_count;
} tetra_codec_t;

// Function declarations

// RTL-SDR interface (rtl_interface.c)
rtl_sdr_t* rtl_sdr_init(tetra_config_t *config);
int rtl_sdr_start(rtl_sdr_t *sdr, void (*callback)(uint8_t *buf, uint32_t len, void *ctx), void *ctx);
void rtl_sdr_stop(rtl_sdr_t *sdr);
void rtl_sdr_cleanup(rtl_sdr_t *sdr);

// TETRA demodulation (tetra_demod.c)
tetra_demod_t* tetra_demod_init(uint32_t sample_rate, detection_params_t *params, detection_status_t *status);
int tetra_demod_process(tetra_demod_t *demod, uint8_t *iq_data, uint32_t len);
bool tetra_detect_burst(tetra_demod_t *demod);
void tetra_demod_cleanup(tetra_demod_t *demod);

// Detection parameters management
detection_params_t* detection_params_init(void);
void detection_params_cleanup(detection_params_t *params);
void detection_params_reset_defaults(detection_params_t *params);

// Detection status management
detection_status_t* detection_status_init(void);
void detection_status_cleanup(detection_status_t *status);
void detection_status_reset(detection_status_t *status);

// TEA1 cryptography (tea1_crypto.c)
void tea1_init(tea1_context_t *ctx, const uint8_t *key, bool use_vulnerability);
void tea1_decrypt_block(tea1_context_t *ctx, const uint8_t *input, uint8_t *output);
void tea1_decrypt_stream(tea1_context_t *ctx, const uint8_t *input, uint8_t *output, size_t len);

// TEA1 vulnerability exploitation (tea1_crack.c)
bool tea1_crack_key(const uint8_t *ciphertext, const uint8_t *known_plaintext,
                    size_t len, tea1_context_t *ctx);
uint32_t tea1_extract_reduced_key(const uint8_t *full_key);
void tea1_brute_force_32bit(const uint8_t *ciphertext, const uint8_t *known_plaintext,
                            size_t len, tea1_context_t *result);

// Signal processing (signal_processing.c)
void convert_uint8_to_float(const uint8_t *input, float *output, uint32_t len);
void quadrature_demod(const float *i, const float *q, float *output, uint32_t len);
void low_pass_filter(float *data, uint32_t len, float cutoff);
float detect_signal_strength(const float *i, const float *q, uint32_t len);

// Audio output (audio_output.c)
audio_output_t* audio_output_init(const char *filename, int sample_rate);
int audio_output_write(audio_output_t *audio, const int16_t *samples, int count);
void audio_output_cleanup(audio_output_t *audio);

// Real-time audio playback (audio_playback.c)
audio_playback_t* audio_playback_init(int sample_rate);
int audio_playback_write(audio_playback_t *playback, const int16_t *samples, int count);
void audio_playback_start(audio_playback_t *playback);
void audio_playback_stop(audio_playback_t *playback);
void audio_playback_cleanup(audio_playback_t *playback);

// TETRA audio codec (tetra_codec.c)
tetra_codec_t* tetra_codec_init(void);
int tetra_codec_decode_frame(tetra_codec_t *codec, const uint8_t *encoded_bits,
                              int16_t *audio_samples);
void tetra_codec_cleanup(tetra_codec_t *codec);

// Utilities (utils.c)
void hex_dump(const uint8_t *data, size_t len, const char *label);
uint64_t get_timestamp_us(void);
void log_message(bool verbose, const char *format, ...);

// GUI interface (gui.c)
typedef struct tetra_gui_t tetra_gui_t;

tetra_gui_t* tetra_gui_init(tetra_config_t *config, detection_params_t *params,
                            detection_status_t *status, rtl_sdr_t *sdr);
void tetra_gui_run(tetra_gui_t *gui);
void tetra_gui_cleanup(tetra_gui_t *gui);
void tetra_gui_update_status(tetra_gui_t *gui);

#endif // TETRA_ANALYZER_H
