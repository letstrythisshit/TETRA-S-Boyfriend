/*
 * TETRA Trunked Radio System - Channel Manager
 * Manages control channel monitoring, voice channel following, and talk groups
 */

#include "tetra_analyzer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

// Channel monitoring thread
static void* channel_monitor_thread(void *arg) {
    channel_manager_t *mgr = (channel_manager_t*)arg;

    log_message(true, "Channel monitor thread started\n");

    while (mgr->running) {
        uint64_t now = get_timestamp_us();

        // Check for control channel timeout
        if ((now - mgr->last_control_msg_time) > (CONTROL_CHANNEL_TIMEOUT * 1000)) {
            log_message(true, "⚠ Warning: No control channel messages for %d seconds\n",
                       CONTROL_CHANNEL_TIMEOUT / 1000);
            mgr->last_control_msg_time = now;
        }

        // Check for expired voice channels
        pthread_mutex_lock(&mgr->channel_lock);
        for (int i = 0; i < mgr->active_channel_count; i++) {
            voice_channel_t *ch = &mgr->voice_channels[i];
            if (ch->active) {
                uint64_t age = now - ch->last_update;
                // Mark channel as inactive if no activity for hold_time
                if (age > (mgr->config.hold_time_ms * 1000)) {
                    log_message(true, "Channel %u (TG %u) timed out after %lu ms\n",
                               ch->frequency, ch->talk_group_id, age / 1000);
                    ch->active = false;

                    // Add to history
                    pthread_mutex_lock(&mgr->history_lock);
                    int idx = mgr->history_head;
                    mgr->history[idx].timestamp = ch->grant_time;
                    mgr->history[idx].talk_group_id = ch->talk_group_id;
                    mgr->history[idx].frequency = ch->frequency;
                    mgr->history[idx].source_id = ch->source_id;
                    mgr->history[idx].duration_ms = (now - ch->grant_time) / 1000;
                    mgr->history_head = (mgr->history_head + 1) % CHANNEL_HISTORY_SIZE;
                    if (mgr->history_count < CHANNEL_HISTORY_SIZE) {
                        mgr->history_count++;
                    }
                    pthread_mutex_unlock(&mgr->history_lock);
                }
            }
        }
        pthread_mutex_unlock(&mgr->channel_lock);

        usleep(100000); // 100ms
    }

    return NULL;
}

// Initialize channel manager
channel_manager_t* channel_manager_init(trunking_config_t *config, rtl_sdr_t *sdr,
                                        detection_params_t *params, detection_status_t *status) {
    channel_manager_t *mgr = calloc(1, sizeof(channel_manager_t));
    if (!mgr) {
        fprintf(stderr, "Failed to allocate channel manager\n");
        return NULL;
    }

    // Copy configuration
    memcpy(&mgr->config, config, sizeof(trunking_config_t));

    // Initialize mutexes
    pthread_mutex_init(&mgr->talk_group_lock, NULL);
    pthread_mutex_init(&mgr->channel_lock, NULL);
    pthread_mutex_init(&mgr->history_lock, NULL);

    // Initialize control channel demodulator
    if (config->control_channel_freq > 0) {
        mgr->control_demod = tetra_demod_init(TETRA_SAMPLE_RATE, params, status);
        if (!mgr->control_demod) {
            fprintf(stderr, "Failed to initialize control channel demodulator\n");
            channel_manager_cleanup(mgr);
            return NULL;
        }
    }

    // Initialize voice channel demodulators
    for (int i = 0; i < MAX_ACTIVE_CHANNELS; i++) {
        mgr->voice_channels[i].active = false;
        mgr->voice_channels[i].demod = NULL;
    }

    mgr->sdr = sdr;
    mgr->current_frequency = config->control_channel_freq;
    mgr->current_channel_idx = -1;
    mgr->running = false;

    log_message(true, "✓ Channel manager initialized\n");
    log_message(true, "  Control channel: %u Hz\n", config->control_channel_freq);
    log_message(true, "  Auto-follow: %s\n", config->auto_follow ? "enabled" : "disabled");
    log_message(true, "  Emergency override: %s\n", config->emergency_override ? "enabled" : "disabled");
    log_message(true, "  Priority threshold: %d\n", config->priority_threshold);

    return mgr;
}

// Start channel manager
void channel_manager_start(channel_manager_t *mgr) {
    if (!mgr) return;

    mgr->running = true;
    mgr->last_control_msg_time = get_timestamp_us();

    // Tune to control channel
    if (mgr->config.control_channel_freq > 0) {
        log_message(true, "Tuning to control channel: %u Hz\n", mgr->config.control_channel_freq);
        channel_manager_tune_to_channel(mgr, mgr->config.control_channel_freq);
    }

    // Start monitoring thread
    if (pthread_create(&mgr->monitor_thread, NULL, channel_monitor_thread, mgr) != 0) {
        fprintf(stderr, "Failed to create channel monitor thread\n");
        mgr->running = false;
        return;
    }

    log_message(true, "✓ Channel manager started\n");
}

// Stop channel manager
void channel_manager_stop(channel_manager_t *mgr) {
    if (!mgr) return;

    log_message(true, "Stopping channel manager...\n");
    mgr->running = false;

    // Wait for monitor thread
    if (mgr->monitor_thread) {
        pthread_join(mgr->monitor_thread, NULL);
    }

    log_message(true, "✓ Channel manager stopped\n");
}

// Cleanup channel manager
void channel_manager_cleanup(channel_manager_t *mgr) {
    if (!mgr) return;

    channel_manager_stop(mgr);

    // Cleanup demodulators
    if (mgr->control_demod) {
        tetra_demod_cleanup(mgr->control_demod);
    }

    for (int i = 0; i < MAX_ACTIVE_CHANNELS; i++) {
        if (mgr->voice_channels[i].demod) {
            tetra_demod_cleanup(mgr->voice_channels[i].demod);
        }
    }

    // Destroy mutexes
    pthread_mutex_destroy(&mgr->talk_group_lock);
    pthread_mutex_destroy(&mgr->channel_lock);
    pthread_mutex_destroy(&mgr->history_lock);

    free(mgr);
}

// Add talk group
int channel_manager_add_talk_group(channel_manager_t *mgr, uint32_t id, const char *name,
                                   bool monitored, int priority) {
    if (!mgr) return -1;

    pthread_mutex_lock(&mgr->talk_group_lock);

    if (mgr->talk_group_count >= MAX_TALK_GROUPS) {
        pthread_mutex_unlock(&mgr->talk_group_lock);
        return -1;
    }

    talk_group_t *tg = &mgr->talk_groups[mgr->talk_group_count];
    tg->id = id;
    strncpy(tg->name, name, sizeof(tg->name) - 1);
    tg->monitored = monitored;
    tg->priority = priority;
    tg->call_count = 0;
    tg->last_activity = 0;

    int idx = mgr->talk_group_count;
    mgr->talk_group_count++;

    pthread_mutex_unlock(&mgr->talk_group_lock);

    log_message(true, "Added talk group %u: %s (priority=%d, monitored=%s)\n",
               id, name, priority, monitored ? "yes" : "no");

    return idx;
}

// Get talk group by ID
talk_group_t* channel_manager_get_talk_group(channel_manager_t *mgr, uint32_t id) {
    if (!mgr) return NULL;

    pthread_mutex_lock(&mgr->talk_group_lock);

    for (int i = 0; i < mgr->talk_group_count; i++) {
        if (mgr->talk_groups[i].id == id) {
            pthread_mutex_unlock(&mgr->talk_group_lock);
            return &mgr->talk_groups[i];
        }
    }

    pthread_mutex_unlock(&mgr->talk_group_lock);
    return NULL;
}

// Set talk group monitored status
void channel_manager_set_talk_group_monitored(channel_manager_t *mgr, uint32_t id, bool monitored) {
    talk_group_t *tg = channel_manager_get_talk_group(mgr, id);
    if (tg) {
        pthread_mutex_lock(&mgr->talk_group_lock);
        tg->monitored = monitored;
        pthread_mutex_unlock(&mgr->talk_group_lock);
        log_message(true, "Talk group %u monitoring: %s\n", id, monitored ? "enabled" : "disabled");
    }
}

// List all talk groups
void channel_manager_list_talk_groups(channel_manager_t *mgr) {
    if (!mgr) return;

    pthread_mutex_lock(&mgr->talk_group_lock);

    printf("\n=== Talk Groups ===\n");
    printf("ID       Name                  Priority  Monitored  Calls\n");
    printf("---------------------------------------------------------------\n");

    for (int i = 0; i < mgr->talk_group_count; i++) {
        talk_group_t *tg = &mgr->talk_groups[i];
        printf("%-8u %-20s %-9d %-10s %u\n",
               tg->id, tg->name, tg->priority,
               tg->monitored ? "YES" : "NO",
               tg->call_count);
    }

    printf("\nTotal: %d talk groups\n\n", mgr->talk_group_count);

    pthread_mutex_unlock(&mgr->talk_group_lock);
}

// Process control channel message
void channel_manager_process_control_message(channel_manager_t *mgr, ctrl_message_t *msg) {
    if (!mgr || !msg) return;

    mgr->last_control_msg_time = get_timestamp_us();
    mgr->control_msg_count++;

    log_message(true, "[CTRL] %s: TG=%u SRC=%u FREQ=%u ENC=%d EMER=%d\n",
               ctrl_msg_type_to_string(msg->type),
               msg->talk_group_id, msg->source_id, msg->channel_freq,
               msg->encrypted, msg->emergency);

    // Update talk group activity
    talk_group_t *tg = channel_manager_get_talk_group(mgr, msg->talk_group_id);
    if (tg) {
        pthread_mutex_lock(&mgr->talk_group_lock);
        tg->last_activity = get_timestamp_us();
        tg->call_count++;
        pthread_mutex_unlock(&mgr->talk_group_lock);
    }

    // Handle different message types
    switch (msg->type) {
        case CTRL_MSG_CHANNEL_GRANT:
        case CTRL_MSG_GROUP_CALL:
            mgr->total_calls++;
            if (msg->emergency) {
                mgr->emergency_calls++;
            }
            if (msg->encrypted) {
                mgr->encrypted_calls++;
            }

            // Check if we should follow this call
            bool should_follow = false;

            // Emergency override
            if (mgr->config.emergency_override && msg->emergency) {
                should_follow = true;
                log_message(true, "🚨 EMERGENCY CALL - Following!\n");
            }

            // Check talk group monitoring
            if (tg && tg->monitored && tg->priority >= mgr->config.priority_threshold) {
                should_follow = true;
            }

            // Record all mode
            if (mgr->config.record_all) {
                should_follow = true;
            }

            // Follow the channel if conditions are met
            if (should_follow && mgr->config.auto_follow && msg->channel_freq > 0) {
                log_message(true, "→ Following to voice channel: %u Hz (TG %u)\n",
                           msg->channel_freq, msg->talk_group_id);

                // Find or create voice channel slot
                pthread_mutex_lock(&mgr->channel_lock);

                int slot = -1;
                for (int i = 0; i < MAX_ACTIVE_CHANNELS; i++) {
                    if (!mgr->voice_channels[i].active) {
                        slot = i;
                        break;
                    }
                }

                if (slot >= 0) {
                    voice_channel_t *ch = &mgr->voice_channels[slot];
                    ch->frequency = msg->channel_freq;
                    ch->talk_group_id = msg->talk_group_id;
                    ch->source_id = msg->source_id;
                    ch->active = true;
                    ch->encrypted = msg->encrypted;
                    ch->grant_time = get_timestamp_us();
                    ch->last_update = ch->grant_time;
                    ch->signal_strength = 0.0f;

                    mgr->active_channel_count++;
                    mgr->current_channel_idx = slot;

                    // Tune SDR to this frequency
                    channel_manager_tune_to_channel(mgr, msg->channel_freq);
                } else {
                    log_message(true, "⚠ No available voice channel slots\n");
                }

                pthread_mutex_unlock(&mgr->channel_lock);
            }
            break;

        case CTRL_MSG_CHANNEL_RELEASE:
            // Mark channel as released
            pthread_mutex_lock(&mgr->channel_lock);
            for (int i = 0; i < MAX_ACTIVE_CHANNELS; i++) {
                if (mgr->voice_channels[i].active &&
                    mgr->voice_channels[i].talk_group_id == msg->talk_group_id) {
                    log_message(true, "Channel released: %u Hz (TG %u)\n",
                               mgr->voice_channels[i].frequency, msg->talk_group_id);
                    mgr->voice_channels[i].active = false;
                    mgr->active_channel_count--;

                    // Return to control channel
                    if (mgr->current_channel_idx == i) {
                        log_message(true, "← Returning to control channel: %u Hz\n",
                                   mgr->config.control_channel_freq);
                        channel_manager_tune_to_channel(mgr, mgr->config.control_channel_freq);
                        mgr->current_channel_idx = -1;
                    }
                    break;
                }
            }
            pthread_mutex_unlock(&mgr->channel_lock);
            break;

        case CTRL_MSG_EMERGENCY:
            log_message(true, "🚨 EMERGENCY from unit %u on TG %u\n",
                       msg->source_id, msg->talk_group_id);
            mgr->emergency_calls++;
            break;

        default:
            break;
    }
}

// Get active channel for talk group
voice_channel_t* channel_manager_get_active_channel(channel_manager_t *mgr, uint32_t talk_group_id) {
    if (!mgr) return NULL;

    pthread_mutex_lock(&mgr->channel_lock);
    for (int i = 0; i < MAX_ACTIVE_CHANNELS; i++) {
        if (mgr->voice_channels[i].active &&
            mgr->voice_channels[i].talk_group_id == talk_group_id) {
            pthread_mutex_unlock(&mgr->channel_lock);
            return &mgr->voice_channels[i];
        }
    }
    pthread_mutex_unlock(&mgr->channel_lock);
    return NULL;
}

// Tune to channel
void channel_manager_tune_to_channel(channel_manager_t *mgr, uint32_t frequency) {
    if (!mgr || !mgr->sdr) return;

    mgr->current_frequency = frequency;

    // Note: Would need to implement rtl_sdr_set_frequency() in rtl_interface.c
    // For now, this is a placeholder that shows the intended behavior
    log_message(true, "Tuning SDR to %u Hz (%.3f MHz)\n",
               frequency, frequency / 1e6);
}

// Print statistics
void channel_manager_print_statistics(channel_manager_t *mgr) {
    if (!mgr) return;

    printf("\n=== Channel Manager Statistics ===\n");
    printf("Control messages received: %u\n", mgr->control_msg_count);
    printf("Total calls: %u\n", mgr->total_calls);
    printf("Emergency calls: %u\n", mgr->emergency_calls);
    printf("Encrypted calls: %u\n", mgr->encrypted_calls);
    printf("Active voice channels: %d\n", mgr->active_channel_count);
    printf("Talk groups tracked: %d\n", mgr->talk_group_count);
    printf("\n");
}

// Print active channels
void channel_manager_print_active_channels(channel_manager_t *mgr) {
    if (!mgr) return;

    pthread_mutex_lock(&mgr->channel_lock);

    printf("\n=== Active Voice Channels ===\n");
    printf("Frequency     Talk Group  Source    Encrypted  Age(s)   Signal\n");
    printf("-------------------------------------------------------------------\n");

    uint64_t now = get_timestamp_us();
    int count = 0;

    for (int i = 0; i < MAX_ACTIVE_CHANNELS; i++) {
        voice_channel_t *ch = &mgr->voice_channels[i];
        if (ch->active) {
            uint64_t age = (now - ch->grant_time) / 1000000;
            printf("%-13u %-11u %-9u %-10s %-8lu %.2f\n",
                   ch->frequency, ch->talk_group_id, ch->source_id,
                   ch->encrypted ? "YES" : "NO",
                   age, ch->signal_strength);
            count++;
        }
    }

    if (count == 0) {
        printf("(No active channels)\n");
    }

    printf("\n");
    pthread_mutex_unlock(&mgr->channel_lock);
}

// Get channel history
channel_history_entry_t* channel_manager_get_history(channel_manager_t *mgr, int *count) {
    if (!mgr || !count) return NULL;

    pthread_mutex_lock(&mgr->history_lock);
    *count = mgr->history_count;
    pthread_mutex_unlock(&mgr->history_lock);

    return mgr->history;
}
