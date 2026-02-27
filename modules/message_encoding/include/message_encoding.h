#ifndef MESSAGE_ENCODING_H
#define MESSAGE_ENCODING_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define PING_PACKET_VERSION

void comms_init(uint32_t trackerId);

void create_v1_tracker_ping_pkt(
    uint8_t *bufferOut, // must be 13 bytes
    const uint8_t *key,
    size_t key_len,
    uint8_t battery_level, // 3 bits
    bool in_emergency_mode, // 1 bit
    uint32_t tracker_id,
    uint32_t counter
);

bool parse_v1_tracker_pong_pkt(
    uint8_t *buffer, // must be 9 bytes
    const uint8_t *key,
    size_t key_len,
    uint8_t* commandOut,
    uint32_t* counterOut
);

#endif