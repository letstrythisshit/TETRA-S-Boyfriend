/*
 * Audio Output Module
 * Handles decoded audio stream output
 */

#include "tetra_analyzer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

audio_output_t* audio_output_init(const char *filename, int sample_rate) {
    audio_output_t *audio = calloc(1, sizeof(audio_output_t));
    if (!audio) {
        fprintf(stderr, "Failed to allocate audio output structure\n");
        return NULL;
    }

    audio->buffer_size = AUDIO_BUFFER_SIZE;
    audio->sample_rate = sample_rate;

    audio->buffer = calloc(audio->buffer_size, sizeof(int16_t));
    if (!audio->buffer) {
        fprintf(stderr, "Failed to allocate audio buffer\n");
        free(audio);
        return NULL;
    }

    // Open output file
    if (filename) {
        audio->output_file = fopen(filename, "wb");
        if (!audio->output_file) {
            fprintf(stderr, "Failed to open audio output file: %s\n", filename);
            free(audio->buffer);
            free(audio);
            return NULL;
        }

        // Write simple WAV header (44 bytes)
        uint8_t wav_header[44];
        memset(wav_header, 0, 44);

        // RIFF header
        memcpy(wav_header, "RIFF", 4);
        // File size (will be updated on close)
        memcpy(wav_header + 8, "WAVE", 4);

        // Format chunk
        memcpy(wav_header + 12, "fmt ", 4);
        uint32_t fmt_size = 16;
        memcpy(wav_header + 16, &fmt_size, 4);

        uint16_t audio_format = 1; // PCM
        uint16_t num_channels = 1; // Mono
        uint32_t sample_rate_u32 = sample_rate;
        uint16_t bits_per_sample = 16;
        uint32_t byte_rate = sample_rate * num_channels * bits_per_sample / 8;
        uint16_t block_align = num_channels * bits_per_sample / 8;

        memcpy(wav_header + 20, &audio_format, 2);
        memcpy(wav_header + 22, &num_channels, 2);
        memcpy(wav_header + 24, &sample_rate_u32, 4);
        memcpy(wav_header + 28, &byte_rate, 4);
        memcpy(wav_header + 32, &block_align, 2);
        memcpy(wav_header + 34, &bits_per_sample, 2);

        // Data chunk header
        memcpy(wav_header + 36, "data", 4);
        // Data size (will be updated on close)

        fwrite(wav_header, 1, 44, audio->output_file);

        log_message(true, "Audio output: %s (%d Hz, 16-bit PCM)\n",
                   filename, sample_rate);
    }

    return audio;
}

int audio_output_write(audio_output_t *audio, const int16_t *samples, int count) {
    if (!audio || !samples || count <= 0) {
        return -1;
    }

    if (audio->output_file) {
        size_t written = fwrite(samples, sizeof(int16_t), count, audio->output_file);
        return (int)written;
    }

    return 0;
}

void audio_output_cleanup(audio_output_t *audio) {
    if (audio) {
        if (audio->output_file) {
            // Update WAV header with correct file size
            long file_size = ftell(audio->output_file);

            if (file_size > 44) {
                fseek(audio->output_file, 4, SEEK_SET);
                uint32_t chunk_size = (uint32_t)(file_size - 8);
                fwrite(&chunk_size, 4, 1, audio->output_file);

                fseek(audio->output_file, 40, SEEK_SET);
                uint32_t data_size = (uint32_t)(file_size - 44);
                fwrite(&data_size, 4, 1, audio->output_file);
            }

            fclose(audio->output_file);
            log_message(true, "Audio output closed (%ld bytes)\n", file_size);
        }

        free(audio->buffer);
        free(audio);
    }
}
