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

// Trunking system constants
#define MAX_TALK_GROUPS 256            // Maximum talk groups to track
#define MAX_ACTIVE_CHANNELS 16         // Maximum simultaneous voice channels
#define CHANNEL_HISTORY_SIZE 100       // Channel assignment history depth
#define CONTROL_CHANNEL_TIMEOUT 5000   // ms without control channel before error

// Forward declarations
typedef struct tetra_demod_t tetra_demod_t;

// Talk group information
typedef struct {
    uint32_t id;                       // Talk group ID
    char name[64];                     // Talk group name/description
    bool monitored;                    // Whether to monitor this group
    uint32_t call_count;               // Number of calls seen
    uint64_t last_activity;            // Timestamp of last activity
    int priority;                      // Priority level (0-10, higher = more important)
} talk_group_t;

// Voice channel state
typedef struct {
    uint32_t frequency;                // Channel frequency
    uint32_t talk_group_id;            // Current talk group using this channel
    uint32_t source_id;                // Radio ID of transmitter
    bool active;                       // Channel currently in use
    bool encrypted;                    // Whether traffic is encrypted
    uint64_t grant_time;               // When channel was granted
    uint64_t last_update;              // Last activity on this channel
    float signal_strength;             // Current signal strength
    tetra_demod_t *demod;              // Dedicated demodulator for this channel
} voice_channel_t;

// Control channel message types
typedef enum {
    CTRL_MSG_CHANNEL_GRANT,            // Voice channel assignment
    CTRL_MSG_CHANNEL_RELEASE,          // Voice channel release
    CTRL_MSG_REGISTRATION,             // Radio registration
    CTRL_MSG_GROUP_CALL,               // Group call setup
    CTRL_MSG_UNIT_TO_UNIT,             // Direct unit-to-unit call
    CTRL_MSG_EMERGENCY,                // Emergency call
    CTRL_MSG_STATUS,                   // Status update
    CTRL_MSG_AFFILIATION,              // Group affiliation change
    CTRL_MSG_UNKNOWN
} ctrl_msg_type_t;

// Control channel message
typedef struct {
    ctrl_msg_type_t type;
    uint32_t talk_group_id;
    uint32_t source_id;
    uint32_t dest_id;
    uint32_t channel_freq;
    bool encrypted;
    bool emergency;
    uint64_t timestamp;
} ctrl_message_t;

// Channel assignment history entry
typedef struct {
    uint64_t timestamp;
    uint32_t talk_group_id;
    uint32_t frequency;
    uint32_t source_id;
    uint32_t duration_ms;
} channel_history_entry_t;

// Trunking system configuration
typedef struct {
    bool enabled;                      // Enable trunking mode
    uint32_t control_channel_freq;     // Control channel frequency
    bool auto_follow;                  // Automatically follow voice channels
    bool record_all;                   // Record all channels or just monitored
    int priority_threshold;            // Minimum priority to follow (0-10)
    uint32_t hold_time_ms;             // How long to hold on channel after end
    bool emergency_override;           // Always follow emergency calls
} trunking_config_t;

// Configuration structure
typedef struct {
    uint32_t frequency;
    uint32_t sample_rate;
    int gain;
    bool auto_gain;
    float squelch_threshold;           // Signal strength threshold for squelch (default: 15.0)
    bool verbose;
    bool use_known_vulnerability;
    bool enable_realtime_audio;
    bool enable_gui;
    bool enable_trunking;              // Enable trunked radio mode
    char *output_file;
    int device_index;
    trunking_config_t trunking;        // Trunking configuration
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
struct tetra_demod_t {
    uint32_t frequency;
    float *i_samples;
    float *q_samples;
    int sample_count;
    float symbol_timing;
    uint8_t *demod_bits;
    int bit_count;
    detection_params_t *params;      // Pointer to shared detection parameters
    detection_status_t *status;      // Pointer to shared status information
};

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

// Channel manager state (trunked radio system)
typedef struct {
    // Configuration
    trunking_config_t config;

    // Control channel
    tetra_demod_t *control_demod;
    uint64_t last_control_msg_time;
    uint32_t control_msg_count;

    // Talk groups
    talk_group_t talk_groups[MAX_TALK_GROUPS];
    int talk_group_count;
    pthread_mutex_t talk_group_lock;

    // Active voice channels
    voice_channel_t voice_channels[MAX_ACTIVE_CHANNELS];
    int active_channel_count;
    pthread_mutex_t channel_lock;

    // Currently followed channel (for single SDR mode)
    int current_channel_idx;
    uint32_t current_frequency;
    rtl_sdr_t *sdr;

    // Channel history
    channel_history_entry_t history[CHANNEL_HISTORY_SIZE];
    int history_head;
    int history_count;
    pthread_mutex_t history_lock;

    // Statistics
    uint32_t total_calls;
    uint32_t emergency_calls;
    uint32_t encrypted_calls;

    // State
    bool running;
    pthread_t monitor_thread;
} channel_manager_t;

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

// Trunked radio system (trunking.c)
channel_manager_t* channel_manager_init(trunking_config_t *config, rtl_sdr_t *sdr,
                                        detection_params_t *params, detection_status_t *status);
void channel_manager_start(channel_manager_t *mgr);
void channel_manager_stop(channel_manager_t *mgr);
void channel_manager_cleanup(channel_manager_t *mgr);

// Talk group management
int channel_manager_add_talk_group(channel_manager_t *mgr, uint32_t id, const char *name,
                                   bool monitored, int priority);
talk_group_t* channel_manager_get_talk_group(channel_manager_t *mgr, uint32_t id);
void channel_manager_set_talk_group_monitored(channel_manager_t *mgr, uint32_t id, bool monitored);
void channel_manager_list_talk_groups(channel_manager_t *mgr);

// Channel operations
void channel_manager_process_control_message(channel_manager_t *mgr, ctrl_message_t *msg);
voice_channel_t* channel_manager_get_active_channel(channel_manager_t *mgr, uint32_t talk_group_id);
void channel_manager_tune_to_channel(channel_manager_t *mgr, uint32_t frequency);

// Statistics and monitoring
void channel_manager_print_statistics(channel_manager_t *mgr);
void channel_manager_print_active_channels(channel_manager_t *mgr);
channel_history_entry_t* channel_manager_get_history(channel_manager_t *mgr, int *count);

// Control channel decoding (control_channel.c)
bool decode_control_channel_data(uint8_t *bits, int bit_count, ctrl_message_t *msg);
const char* ctrl_msg_type_to_string(ctrl_msg_type_t type);

#endif // TETRA_ANALYZER_H
