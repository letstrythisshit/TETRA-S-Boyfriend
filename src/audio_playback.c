/*
 * Real-time Audio Playback Module
 * ALSA-based real-time audio output with ring buffer
 *
 * Allows hearing decrypted TETRA audio in real-time during laboratory testing
 */

#include "tetra_analyzer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef HAVE_ALSA
#include <alsa/asoundlib.h>
#endif

// Playback thread function
static void* playback_thread_func(void *arg) {
    audio_playback_t *playback = (audio_playback_t *)arg;

#ifdef HAVE_ALSA
    snd_pcm_t *pcm = (snd_pcm_t *)playback->pcm_handle;
    int16_t temp_buffer[512];  // Small chunks for low latency

    log_message(true, "Real-time audio playback thread started\n");

    while (playback->running) {
        pthread_mutex_lock(&playback->buffer_mutex);

        // Calculate available samples in ring buffer
        int available = 0;
        if (playback->ring_write_pos >= playback->ring_read_pos) {
            available = playback->ring_write_pos - playback->ring_read_pos;
        } else {
            available = playback->ring_size - playback->ring_read_pos + playback->ring_write_pos;
        }

        // Read a chunk if enough data available
        if (available >= 512) {
            for (int i = 0; i < 512; i++) {
                temp_buffer[i] = playback->ring_buffer[playback->ring_read_pos];
                playback->ring_read_pos = (playback->ring_read_pos + 1) % playback->ring_size;
            }
            pthread_mutex_unlock(&playback->buffer_mutex);

            // Play the audio
            int frames = snd_pcm_writei(pcm, temp_buffer, 512);
            if (frames < 0) {
                frames = snd_pcm_recover(pcm, frames, 0);
            }
            if (frames < 0) {
                log_message(true, "ALSA write error: %s\n", snd_strerror(frames));
            }
        } else {
            pthread_mutex_unlock(&playback->buffer_mutex);
            usleep(10000); // 10ms - wait for more data
        }
    }

    log_message(true, "Real-time audio playback thread stopped\n");
#endif

    return NULL;
}

audio_playback_t* audio_playback_init(int sample_rate) {
#ifdef HAVE_ALSA
    audio_playback_t *playback = calloc(1, sizeof(audio_playback_t));
    if (!playback) {
        fprintf(stderr, "Failed to allocate audio playback structure\n");
        return NULL;
    }

    playback->sample_rate = sample_rate;
    playback->ring_size = AUDIO_RING_BUFFER_SIZE;
    playback->ring_write_pos = 0;
    playback->ring_read_pos = 0;
    playback->running = false;

    // Allocate ring buffer
    playback->ring_buffer = calloc(playback->ring_size, sizeof(int16_t));
    if (!playback->ring_buffer) {
        fprintf(stderr, "Failed to allocate audio ring buffer\n");
        free(playback);
        return NULL;
    }

    // Initialize mutex
    pthread_mutex_init(&playback->buffer_mutex, NULL);

    // Open ALSA device
    snd_pcm_t *pcm;
    int err = snd_pcm_open(&pcm, "default", SND_PCM_STREAM_PLAYBACK, 0);
    if (err < 0) {
        fprintf(stderr, "Failed to open ALSA device: %s\n", snd_strerror(err));
        free(playback->ring_buffer);
        free(playback);
        return NULL;
    }

    playback->pcm_handle = pcm;

    // Configure ALSA
    snd_pcm_hw_params_t *params;
    snd_pcm_hw_params_alloca(&params);
    snd_pcm_hw_params_any(pcm, params);

    snd_pcm_hw_params_set_access(pcm, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(pcm, params, SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_channels(pcm, params, 1);  // Mono

    unsigned int rate = sample_rate;
    snd_pcm_hw_params_set_rate_near(pcm, params, &rate, 0);

    // Low latency buffer settings
    snd_pcm_uframes_t buffer_size = 4096;
    snd_pcm_uframes_t period_size = 512;
    snd_pcm_hw_params_set_buffer_size_near(pcm, params, &buffer_size);
    snd_pcm_hw_params_set_period_size_near(pcm, params, &period_size, 0);

    err = snd_pcm_hw_params(pcm, params);
    if (err < 0) {
        fprintf(stderr, "Failed to set ALSA parameters: %s\n", snd_strerror(err));
        snd_pcm_close(pcm);
        free(playback->ring_buffer);
        free(playback);
        return NULL;
    }

    snd_pcm_prepare(pcm);

    log_message(true, "✓ Real-time audio playback initialized (%d Hz, ALSA)\n", sample_rate);
    log_message(true, "  Ring buffer: %d samples (%.1f sec)\n",
                playback->ring_size, (float)playback->ring_size / sample_rate);

    return playback;
#else
    log_message(true, "⚠️  ALSA not available - real-time audio disabled\n");
    log_message(true, "   To enable: sudo apt-get install libasound2-dev && rebuild\n");
    (void)sample_rate;  // Suppress unused warning
    return NULL;
#endif
}

void audio_playback_start(audio_playback_t *playback) {
    if (!playback) return;

    playback->running = true;
    pthread_create(&playback->playback_thread, NULL, playback_thread_func, playback);

    log_message(true, "🔊 Real-time audio playback started - you should hear audio now!\n");
}

void audio_playback_stop(audio_playback_t *playback) {
    if (!playback) return;

    playback->running = false;
    if (playback->playback_thread) {
        pthread_join(playback->playback_thread, NULL);
    }
}

int audio_playback_write(audio_playback_t *playback, const int16_t *samples, int count) {
    if (!playback || !samples || count <= 0) {
        return -1;
    }

    pthread_mutex_lock(&playback->buffer_mutex);

    int written = 0;
    for (int i = 0; i < count; i++) {
        playback->ring_buffer[playback->ring_write_pos] = samples[i];
        playback->ring_write_pos = (playback->ring_write_pos + 1) % playback->ring_size;
        written++;

        // Check for buffer overflow
        if (playback->ring_write_pos == playback->ring_read_pos) {
            // Buffer full - drop oldest sample
            playback->ring_read_pos = (playback->ring_read_pos + 1) % playback->ring_size;
        }
    }

    pthread_mutex_unlock(&playback->buffer_mutex);

    return written;
}

void audio_playback_cleanup(audio_playback_t *playback) {
    if (playback) {
        audio_playback_stop(playback);

#ifdef HAVE_ALSA
        if (playback->pcm_handle) {
            snd_pcm_drain((snd_pcm_t *)playback->pcm_handle);
            snd_pcm_close((snd_pcm_t *)playback->pcm_handle);
        }
#endif

        pthread_mutex_destroy(&playback->buffer_mutex);
        free(playback->ring_buffer);
        free(playback);

        log_message(true, "Real-time audio playback closed\n");
    }
}
