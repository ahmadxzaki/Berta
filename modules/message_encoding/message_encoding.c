#include "include/message_encoding.h"

#include <stdint.h>

#include "hashes/aes128_cmac.h"

uint32_t calculate_cmac(
    const uint8_t *data, 
    size_t data_len,
    const uint8_t *key,
    size_t key_len
) {
    aes128_cmac_context_t ctx;
    
    uint8_t mac_full[AES128_CMAC_BLOCK_SIZE]; 
    
    aes128_cmac_init(&ctx, key, key_len);
    
    // compute the CMAC 
    aes128_cmac_update(&ctx, data, data_len);
    aes128_cmac_final(&ctx, mac_full);

    // truncate to 32-bit (take the first 4 bytes)
    uint32_t truncated_mic = ((uint32_t)mac_full[0] << 24) |
                             ((uint32_t)mac_full[1] << 16) |
                             ((uint32_t)mac_full[2] << 8)  |
                             ((uint32_t)mac_full[3]);
                             
    return truncated_mic;
}

// bufferOut must be 13 bytes
void create_v1_tracker_ping_pkt(
    uint8_t *bufferOut, // must be 13 bytes
    const uint8_t *key,
    size_t key_len,
    uint8_t battery_level, // 3 bits
    bool in_emergency_mode, // 1 bit
    uint32_t tracker_id,
    uint32_t counter
) {
    uint8_t pkt_version = 1;

    bufferOut[0] = (pkt_version & 0x07) | ((battery_level & 0x07) << 3) | ((in_emergency_mode & 0x1) << 6) | 0;

    bufferOut[1] = tracker_id       & 0xFF;
    bufferOut[2] = tracker_id >> 8  & 0xFF;
    bufferOut[3] = tracker_id >> 16 & 0xFF;
    bufferOut[4] = tracker_id >> 24 & 0xFF;

    bufferOut[5] = counter       & 0xFF;
    bufferOut[6] = counter >> 8  & 0xFF;
    bufferOut[7] = counter >> 16 & 0xFF;
    bufferOut[8] = counter >> 24 & 0xFF;

    uint32_t cmac = calculate_cmac(bufferOut, 9, key, key_len);

    bufferOut[9] =  cmac       & 0xFF;
    bufferOut[10] = cmac >> 8  & 0xFF;
    bufferOut[11] = cmac >> 16 & 0xFF;
    bufferOut[12] = cmac >> 24 & 0xFF;

    printf("created packet: ");
    for (int i = 0; i < 13; i++) {
        // Use "%02X" for two uppercase hex digits, with leading zeros
        printf("%02X ", bufferOut[i]);
    }
    printf("\n");
}

bool parse_v1_tracker_pong_pkt(
    uint8_t *buffer, // must be 9 bytes
    const uint8_t *key,
    size_t key_len,
    uint8_t* commandOut,
    uint32_t* counterOut
) {
    uint32_t pktCmac = ((uint32_t)buffer[8] << 24) | 
                       ((uint32_t)buffer[7] << 16) | 
                       ((uint32_t)buffer[6] << 8)  |
                       ((uint32_t)buffer[5]);

    uint32_t expectedCmac = calculate_cmac(buffer, 5, key, key_len);

    if (pktCmac != expectedCmac) {
        return false;
    }
    
    *commandOut = buffer[0];
    
    // extract counter from bytes 1, 2, 3, and 4 (Little Endian)
    *counterOut = ((uint32_t)buffer[4] << 24) | 
                  ((uint32_t)buffer[3] << 16) | 
                  ((uint32_t)buffer[2] << 8)  |
                  ((uint32_t)buffer[1]);
    
    return true;
}