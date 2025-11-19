/*
 * TETRA Control Channel Decoder
 * Decodes signaling messages from TETRA trunked radio control channels
 */

#include "tetra_analyzer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// TETRA PDU types (simplified)
#define PDU_TYPE_D_CHANNEL_GRANT          0x01
#define PDU_TYPE_D_CHANNEL_RELEASE        0x02
#define PDU_TYPE_D_GROUP_CALL             0x03
#define PDU_TYPE_D_UNIT_TO_UNIT          0x04
#define PDU_TYPE_D_REGISTRATION           0x05
#define PDU_TYPE_D_EMERGENCY              0x06
#define PDU_TYPE_D_AFFILIATION            0x07
#define PDU_TYPE_D_STATUS                 0x08

// Extract bits from byte array
static uint32_t extract_bits(const uint8_t *data, int start_bit, int num_bits) {
    uint32_t result = 0;

    for (int i = 0; i < num_bits; i++) {
        int byte_idx = (start_bit + i) / 8;
        int bit_idx = 7 - ((start_bit + i) % 8);

        if (data[byte_idx] & (1 << bit_idx)) {
            result |= (1 << (num_bits - 1 - i));
        }
    }

    return result;
}

// Convert message type to string
const char* ctrl_msg_type_to_string(ctrl_msg_type_t type) {
    switch (type) {
        case CTRL_MSG_CHANNEL_GRANT:    return "CHANNEL_GRANT";
        case CTRL_MSG_CHANNEL_RELEASE:  return "CHANNEL_RELEASE";
        case CTRL_MSG_REGISTRATION:     return "REGISTRATION";
        case CTRL_MSG_GROUP_CALL:       return "GROUP_CALL";
        case CTRL_MSG_UNIT_TO_UNIT:     return "UNIT_TO_UNIT";
        case CTRL_MSG_EMERGENCY:        return "EMERGENCY";
        case CTRL_MSG_STATUS:           return "STATUS";
        case CTRL_MSG_AFFILIATION:      return "AFFILIATION";
        default:                        return "UNKNOWN";
    }
}

// Decode TETRA control channel data
// This is a simplified decoder for educational purposes
// Real TETRA decoding requires full protocol implementation
bool decode_control_channel_data(uint8_t *bits, int bit_count, ctrl_message_t *msg) {
    if (!bits || !msg || bit_count < 64) {
        return false;
    }

    // Initialize message
    memset(msg, 0, sizeof(ctrl_message_t));
    msg->timestamp = get_timestamp_us();
    msg->type = CTRL_MSG_UNKNOWN;

    // Convert bits to bytes for easier processing
    int byte_count = (bit_count + 7) / 8;
    uint8_t *bytes = calloc(byte_count, sizeof(uint8_t));
    if (!bytes) return false;

    for (int i = 0; i < bit_count; i++) {
        int byte_idx = i / 8;
        int bit_idx = 7 - (i % 8);
        if (bits[i]) {
            bytes[byte_idx] |= (1 << bit_idx);
        }
    }

    // Extract PDU type (simplified - first 8 bits)
    uint8_t pdu_type = extract_bits(bytes, 0, 8);

    // Decode based on PDU type
    switch (pdu_type) {
        case PDU_TYPE_D_CHANNEL_GRANT:
            msg->type = CTRL_MSG_CHANNEL_GRANT;
            // Extract talk group ID (bits 8-23, 16 bits)
            msg->talk_group_id = extract_bits(bytes, 8, 16);
            // Extract source ID (bits 24-47, 24 bits)
            msg->source_id = extract_bits(bytes, 24, 24);
            // Extract channel frequency offset (bits 48-59, 12 bits)
            uint32_t freq_offset = extract_bits(bytes, 48, 12);
            // Convert to actual frequency (simplified calculation)
            // TETRA channels are typically 25 kHz apart
            msg->channel_freq = 420000000 + (freq_offset * 25000);
            // Extract encryption flag (bit 60)
            msg->encrypted = extract_bits(bytes, 60, 1);
            // Extract emergency flag (bit 61)
            msg->emergency = extract_bits(bytes, 61, 1);
            break;

        case PDU_TYPE_D_CHANNEL_RELEASE:
            msg->type = CTRL_MSG_CHANNEL_RELEASE;
            msg->talk_group_id = extract_bits(bytes, 8, 16);
            break;

        case PDU_TYPE_D_GROUP_CALL:
            msg->type = CTRL_MSG_GROUP_CALL;
            msg->talk_group_id = extract_bits(bytes, 8, 16);
            msg->source_id = extract_bits(bytes, 24, 24);
            msg->emergency = extract_bits(bytes, 48, 1);
            break;

        case PDU_TYPE_D_UNIT_TO_UNIT:
            msg->type = CTRL_MSG_UNIT_TO_UNIT;
            msg->source_id = extract_bits(bytes, 8, 24);
            msg->dest_id = extract_bits(bytes, 32, 24);
            msg->encrypted = extract_bits(bytes, 56, 1);
            break;

        case PDU_TYPE_D_REGISTRATION:
            msg->type = CTRL_MSG_REGISTRATION;
            msg->source_id = extract_bits(bytes, 8, 24);
            msg->talk_group_id = extract_bits(bytes, 32, 16);
            break;

        case PDU_TYPE_D_EMERGENCY:
            msg->type = CTRL_MSG_EMERGENCY;
            msg->source_id = extract_bits(bytes, 8, 24);
            msg->talk_group_id = extract_bits(bytes, 32, 16);
            msg->emergency = true;
            break;

        case PDU_TYPE_D_AFFILIATION:
            msg->type = CTRL_MSG_AFFILIATION;
            msg->source_id = extract_bits(bytes, 8, 24);
            msg->talk_group_id = extract_bits(bytes, 32, 16);
            break;

        case PDU_TYPE_D_STATUS:
            msg->type = CTRL_MSG_STATUS;
            msg->source_id = extract_bits(bytes, 8, 24);
            break;

        default:
            msg->type = CTRL_MSG_UNKNOWN;
            free(bytes);
            return false;
    }

    free(bytes);
    return true;
}
