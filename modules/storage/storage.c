#include "include/storage.h"
#include "tracker.h"
#include "fram.h"

#include <stdio.h>
#include <stdint.h>

#define STORAGE_MAGIC 0x45504943 // "EPIC" in ASCII

const uint32_t STORAGE_OFFSET = 0x0;

#define STATE_PACKET_SIZE 6
#define STORAGE_PACKET_SIZE 30 // (3 * 4 byte magics) + (3 * 6 byte states)

void serialize_state(const tracker_state_t *state, uint8_t *buffer);
void deserialize_state(const uint8_t *buffer, tracker_state_t *state);
void write_magic(uint8_t *buf, uint32_t magic);
uint32_t read_magic(const uint8_t *buf);

bool storage_init(void) {
    int fram_status = fram_init();
    if (fram_status != 0) {
        printf("fram failed to init, shutting down");
        return false;
    }

    return true;
}

bool backup_state(tracker_state_t state) {
    uint8_t buffer[STORAGE_PACKET_SIZE];
    
    // pack the 3 magics into the first 12 bytes
    write_magic(&buffer[0], STORAGE_MAGIC);
    write_magic(&buffer[4], STORAGE_MAGIC);
    write_magic(&buffer[8], STORAGE_MAGIC);

    // serialize the tracker state directly into the 3 redundancy slots
    serialize_state(&state, &buffer[12]);
    serialize_state(&state, &buffer[18]);
    serialize_state(&state, &buffer[24]);

    // write to FRAM
    int result = fram_write(STORAGE_OFFSET, buffer, STORAGE_PACKET_SIZE);
    return result == 0;
}

bool load_state(tracker_state_t *stateOut) {
    uint8_t buffer[STORAGE_PACKET_SIZE];
    
    // read the 30 bytes from FRAM
    int result = fram_read(STORAGE_OFFSET, buffer, STORAGE_PACKET_SIZE);
    if (result != 0) {
        printf("failed to read state\n");
        return false;
    }

    // extract the magics
    uint32_t m1 = read_magic(&buffer[0]);
    uint32_t m2 = read_magic(&buffer[4]);
    uint32_t m3 = read_magic(&buffer[8]);

    // recovery of the magics via bitwise majority vote
    uint32_t clean_magic = (m1 & m2) | (m2 & m3) | (m1 & m3);

    if (clean_magic != STORAGE_MAGIC) {
        printf("invalid storage signature\n");
        return false;
    }

    // pointers to the 3 serialized state copies in the buffer
    uint8_t *s1 = &buffer[12];
    uint8_t *s2 = &buffer[18];
    uint8_t *s3 = &buffer[24];
    
    uint8_t recovered_state_buf[STATE_PACKET_SIZE];

    // recovery of the state bytes via bitwise majority vote
    for (size_t i = 0; i < STATE_PACKET_SIZE; i++) {
        recovered_state_buf[i] = (s1[i] & s2[i]) | (s2[i] & s3[i]) | (s1[i] & s3[i]);
    }

    // unpack the perfectly clean, voted bytes back into the C struct
    deserialize_state(recovered_state_buf, stateOut);

    return true;
}

/**
 * @brief Serializes the tracker state into a 6-byte buffer for storage
 */
void serialize_state(const tracker_state_t *state, uint8_t *buffer) {
    // byte 0: emergency mode (1 or 0)
    buffer[0] = state->in_emergency_mode ? 1 : 0;
    
    // bytes 1-4: counter (little endian)
    buffer[1] = (state->counter >>  0) & 0xFF;
    buffer[2] = (state->counter >>  8) & 0xFF;
    buffer[3] = (state->counter >> 16) & 0xFF;
    buffer[4] = (state->counter >> 24) & 0xFF;
    
    // byte 5: missed ping count
    buffer[5] = state->missed_truck_reply_count;
}

/**
 * @brief Deserializes a 6-byte buffer back into a tracker state struct
 */
void deserialize_state(const uint8_t *buffer, tracker_state_t *state) {
    // byte 0: emergency mode
    state->in_emergency_mode = (buffer[0] != 0);
    
    // bytes 1-4: counter (little endian)
    state->counter = ((uint32_t)buffer[1] <<  0) |
                     ((uint32_t)buffer[2] <<  8) |
                     ((uint32_t)buffer[3] << 16) |
                     ((uint32_t)buffer[4] << 24);
                     
    // byte 5: missed ping count
    state->missed_truck_reply_count = buffer[5];
}

// Helper to write a 32-bit magic to a buffer
void write_magic(uint8_t *buf, uint32_t magic) {
    buf[0] = (magic >>  0) & 0xFF;
    buf[1] = (magic >>  8) & 0xFF;
    buf[2] = (magic >> 16) & 0xFF;
    buf[3] = (magic >> 24) & 0xFF;
}

// Helper to read a 32-bit magic from a buffer
uint32_t read_magic(const uint8_t *buf) {
    return ((uint32_t)buf[0] <<  0) |
           ((uint32_t)buf[1] <<  8) |
           ((uint32_t)buf[2] << 16) |
           ((uint32_t)buf[3] << 24);
}