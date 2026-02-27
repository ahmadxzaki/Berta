#include "include/tracker.h"
#include "message_encoding.h"
#include "storage.h"
// #include "lora.h"

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "acme_lora.h"
#include "periph/adc.h"
#include "periph/hwrng.h"
#include "random.h"
#include "saml21_backup_mode.h"
#include "msg.h"
#include "ztimer.h"
#include "acme_lora.h"

// DEVICE SPECIFIC CONFIG

#ifndef CONFIG_TRACKER_ID
#define CONFIG_TRACKER_ID 0
#endif

#ifndef CONFIG_TRACKER_KEY
#define CONFIG_TRACKER_KEY 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
#endif

static const uint8_t SESSION_KEY[16] = { CONFIG_TRACKER_KEY };
static const uint32_t TRACKER_ID = CONFIG_TRACKER_ID;

// I/O PINS

#define EXTWAKE \
    { .pin = EXTWAKE_PIN6, \
      .polarity = EXTWAKE_HIGH, \
      .flags = EXTWAKE_IN_PU \
    }
static saml21_extwake_t extwake = EXTWAKE;

#define ADC_VCC 0

// CONFIG

#define TRUCK_MODE_SEND_ATTEMPTS 5
#define TRUCK_MODE_SEND_ATTEMPT_BACKOFF_MIN_MS 200
#define TRUCK_MODE_SEND_ATTEMPT_BACKOFF_MAX_MS 1000

#define MAX_HANDSHAKE_ATTEMPTS_BEFORE_EMERGENCY_MODE 5
#define HANDSHAKE_BACKOFF_MIN_SECONDS 5
#define HANDSHAKE_BACKOFF_MAX_SECONDS 5

#define EMERGENCY_MODE_SEND_ATTEMPTS 5
#define EMERGENCY_MODE_SEND_ATTEMPT_BACKOFF_MIN_MS 200
#define EMERGENCY_MODE_SEND_ATTEMPT_BACKOFF_MAX_MS 1000

#define TRUCK_UNIT_REPLY_SLEEP_MS 400
#define TRUCK_UNIT_REPLY_TIMEOUT_MS 600

#define SUCCESSFUL_HANDSHAKE_SLEEP_SECONDS 10
#define EMERGENCY_MODE_SLEEP_SECONDS 10

// FORWARD DECLARATIONS

bool handle_emergency_mode(tracker_state_t *state);
bool handle_truck_mode(tracker_state_t *state);
bool truck_unit_handshake(tracker_state_t *state);
bool send_emergency_ping(tracker_state_t* state);
bool new_packet(tracker_state_t *state, uint8_t *payloadOut);
bool send_ping(uint8_t* ping_pkt_payload, size_t len);
void tracker_lora_rx_cb(void *buffer, size_t len, int16_t *rssi, int8_t *snr);
int wait_for_pong(uint32_t timeout_ms);
uint32_t random_between(uint32_t lower, uint32_t upper);
uint8_t pack_battery_level(int32_t battery_mv);

// GIPITY LORA RX STUFF

// Custom message type so our thread knows it's a LoRa packet
#define MSG_TYPE_RX_DONE (0x1122)

// State variables for the bridging the callback and the main thread
static kernel_pid_t _tracker_thread_pid = KERNEL_PID_UNDEF;
static uint8_t _rx_buffer[MAX_PACKET_LEN];
static size_t _rx_len = 0;
// Add this: A small mailbox so the timer interrupt can send us the timeout message
#define TRACKER_MSG_QUEUE_SIZE 8
static msg_t _tracker_msg_queue[TRACKER_MSG_QUEUE_SIZE];

bool tracker_init(void) {
    bool sucessfully_initialized_storage = storage_init();
    if (!sucessfully_initialized_storage) {
        printf("failed to initialize storage");
        return false;
    }

    tracker_state_t initial_state = {
        .in_emergency_mode = false,
        .counter = 0,
        .missed_truck_reply_count = 0,
    };
    bool did_backup_state = backup_state(initial_state);
    if (!did_backup_state) {
        printf("failed to store initialized state\n");
        return false;
    }

    printf("initialized tracker\n");
    return tracker_wakeup();
}

bool tracker_wakeup(void) {
    printf("waking up tracker\n");
    msg_init_queue(_tracker_msg_queue, TRACKER_MSG_QUEUE_SIZE);

    bool sucessfully_initialized_storage = storage_init();
    if (!sucessfully_initialized_storage) {
        printf("failed to initialize storage\n");
        return false;
    }

    int adc_init_result = adc_init(ADC_VCC);
    if (adc_init_result != 0) {
        printf("failed to initialize battery voltage adc\n");
        return false;
    }
    
    tracker_state_t state;
    bool sucessfully_loaded_state = load_state(&state);
    if (!sucessfully_loaded_state) {
        printf("failed to load state\n");
        return false;
    }
    printf("loaded state: emerg_mode: %d, counter: %ld, missed_reply: %d\n", state.in_emergency_mode, state.counter, state.missed_truck_reply_count);

    if (state.in_emergency_mode) {
        bool handled_emergency_mode = handle_emergency_mode(&state);
        if (!handled_emergency_mode) {
            printf("failed to handle emergency mode\n");
            return false;
        }
    } else {
        bool handled_truck_mode = handle_truck_mode(&state);
        if (!handled_truck_mode) {
            printf("failed to handle truck mode\n");
            return false;
        }
    }

    return true;
}

bool handle_emergency_mode(tracker_state_t *state) {
    printf("handling emergency mode\n");
    bool sent_emergency_ping = send_emergency_ping(state);
    if (!sent_emergency_ping) {
        printf("failed to send emergency ping\n");
    }

    printf("trying to do handshake to recover from emergency mode\n");
    bool handshake_successful = truck_unit_handshake(state);
    printf("did handshake to try to recover\n");
    if (handshake_successful) {

        state->missed_truck_reply_count = 0;
        state->in_emergency_mode = false;
        bool did_backup_state = backup_state(*state);
        if (!did_backup_state) {
            printf("failed to back up state when recovering from emergency mode");
        }

        
        printf("successful handshake, sleeping for %d\n", EMERGENCY_MODE_SLEEP_SECONDS);
        saml21_backup_mode_enter(1, extwake, SUCCESSFUL_HANDSHAKE_SLEEP_SECONDS, 1);
        return true;
    }

    printf("failed recovery handshake, sleeping for %d\n", EMERGENCY_MODE_SLEEP_SECONDS);
    saml21_backup_mode_enter(1, extwake, EMERGENCY_MODE_SLEEP_SECONDS, 1);
    return true;
} 

bool handle_truck_mode(tracker_state_t *state) {
    printf("handling truck mode\n");
    bool handshake_successful = truck_unit_handshake(state);
    printf("did truck mode handshake\n");
    if (!handshake_successful) {

        state->missed_truck_reply_count++;
        if (state->missed_truck_reply_count > MAX_HANDSHAKE_ATTEMPTS_BEFORE_EMERGENCY_MODE) {
            state->in_emergency_mode = true;
        }
        bool did_back_up_state = backup_state(*state);
        if (!did_back_up_state) {
            printf("failed to back up state after failed truck mode handshake\n");
        }
        
        uint32_t handshake_backoff_seconds = random_between(HANDSHAKE_BACKOFF_MIN_SECONDS, HANDSHAKE_BACKOFF_MAX_SECONDS);

        printf("failed handshake, sleeping for %ld seconds\n", handshake_backoff_seconds);
        saml21_backup_mode_enter(1, extwake, handshake_backoff_seconds, 1);
        return false;
    }

    state->missed_truck_reply_count = 0;
    bool did_back_up_state = backup_state(*state);
    if (!did_back_up_state) {
        printf("failed to back up state after successful truck mode handshake\n");
    }

    printf("successful handshake, sleeping for %d seconds\n", SUCCESSFUL_HANDSHAKE_SLEEP_SECONDS);
    saml21_backup_mode_enter(1, extwake, SUCCESSFUL_HANDSHAKE_SLEEP_SECONDS, 1);
    return true;
}


bool truck_unit_handshake(tracker_state_t *state) {
    uint8_t ping_pkt_payload[13];
    bool created_new_packet = new_packet(state, ping_pkt_payload);
    if (!created_new_packet) {
        printf("failed to create a new packet for truck unit\n");
        return false;
    }

    lora_state_t handshake_lora = {0};

    handshake_lora.bandwidth        = LORA_BW_125_KHZ; // kHz
    handshake_lora.spreading_factor = LORA_SF7; // SF7
    handshake_lora.coderate         = LORA_CR_4_5; // 4/5 TODO: check that 0 is actually 4/5
    handshake_lora.channel          = 915000000; // Hz
    handshake_lora.power            = 13; // dBm
    handshake_lora.boost            = false;
    handshake_lora.data_cb = (lora_data_cb_t *)tracker_lora_rx_cb;

    bool sucessfully_sent_truck_ping = false;
    for (int send_attempt = 0; send_attempt < TRUCK_MODE_SEND_ATTEMPTS; send_attempt++) {
        printf("attempt %d sending ping to truck unit\n", send_attempt);
        int lora_init_result = lora_init(&handshake_lora);
        if (lora_init_result != 0) {
            printf("lora_init failed: %d\n", lora_init_result);
            return false;
        }

        
        sucessfully_sent_truck_ping = send_ping(ping_pkt_payload, sizeof(ping_pkt_payload));
        if (sucessfully_sent_truck_ping) {
            break;
        }

        lora_off();

        printf("failed to send truck mode ping, retrying\n");
        uint32_t random_backoff_ms = random_between(TRUCK_MODE_SEND_ATTEMPT_BACKOFF_MIN_MS, TRUCK_MODE_SEND_ATTEMPT_BACKOFF_MAX_MS);
        ztimer_sleep(ZTIMER_MSEC, random_backoff_ms); // change to zsleep?
    }
    lora_off();
    if (!sucessfully_sent_truck_ping) {
        printf("ran out of attempts to send truck ping\n");
        return false;
    }
    printf("sent truck ping\n");
    
    // sleep
    ztimer_sleep(ZTIMER_MSEC, TRUCK_UNIT_REPLY_SLEEP_MS);

    int lora_wake_result = lora_init(&handshake_lora);
    if (lora_wake_result != 0) {
        printf("failed to wake radio for listening\n");
        return false;
    }
    int rx_len = wait_for_pong(TRUCK_UNIT_REPLY_TIMEOUT_MS);
    printf("waited for truck pong\n");
    if (rx_len < 0) {
        printf("timed out when waiting for truck unit reply\n");
        return false;
    }
    lora_off();

    if (rx_len != 9) {
        printf("invalid received packet length\n");
        return false;
    }

    // wait for a packet
    uint8_t cmd;
    uint32_t echoed_counter;

    bool got_authentic_packet = parse_v1_tracker_pong_pkt(
        _rx_buffer,
        SESSION_KEY,
        sizeof(SESSION_KEY),
        &cmd, 
        &echoed_counter
    );

    if (!got_authentic_packet) {
        printf("packet failed mac check\n");
        return false;
    }

    if (echoed_counter != state->counter) {
        printf("incorrect packet counter (replay attack)\n");
        return false;
    }

    printf("received pong with command byte: %x\n", cmd);

    return true;
}

bool send_emergency_ping(tracker_state_t* state) {
    uint8_t ping_pkt_payload[13];
    bool created_new_packet = new_packet(state, ping_pkt_payload);
    if (!created_new_packet) {
        printf("failed to create a new packet to send emergency ping\n");
        return false;
    }

    lora_state_t emergency_lora = {0};

    emergency_lora.bandwidth        = LORA_BW_125_KHZ;
    emergency_lora.spreading_factor = LORA_SF12;
    emergency_lora.coderate         = LORA_CR_4_8;
    emergency_lora.channel          = 915000000;
    emergency_lora.power            = 8; // dBm
    emergency_lora.boost            = true;
    emergency_lora.data_cb          = (lora_data_cb_t *)tracker_lora_rx_cb;

    bool sucessfully_sent_truck_ping = false;
    for (int send_attempt = 0; send_attempt < EMERGENCY_MODE_SEND_ATTEMPTS; send_attempt++) {
        int lora_init_result = lora_init(&emergency_lora);
        if (lora_init_result != 0) {
            printf("lora_init failed: %d\n", lora_init_result);
            return false;
        }
        
        sucessfully_sent_truck_ping = send_ping(ping_pkt_payload, sizeof(ping_pkt_payload));
        if (sucessfully_sent_truck_ping) {
            break;
        }

        lora_off();

        printf("failed to send emergency mode ping, retrying\n");
        uint32_t random_backoff_ms = random_between(EMERGENCY_MODE_SEND_ATTEMPT_BACKOFF_MIN_MS, EMERGENCY_MODE_SEND_ATTEMPT_BACKOFF_MAX_MS);
        ztimer_sleep(ZTIMER_MSEC, random_backoff_ms);
    }
    lora_off();
    if (!sucessfully_sent_truck_ping) {
        printf("ran out of attempts to send emergency ping\n");
        return false;
    }
    printf("sent emergency mode ping\n");

    return true;
}

bool new_packet(
    tracker_state_t *state,
    uint8_t *payloadOut // must be 13 bytes
) {
    int32_t battery_mv = adc_sample(ADC_VCC, ADC_RES_12BIT) * 4000 / 4095;
    uint8_t battery_level = pack_battery_level(battery_mv);
    
    state->counter++;
    bool did_back_up_state = backup_state(*state);
    if (!did_back_up_state) {
        printf("failed to back up state after incrementing packet counter (fatal)\n");
        return false;
    }

    create_v1_tracker_ping_pkt(
        payloadOut,
        SESSION_KEY,
        sizeof(SESSION_KEY),
        battery_level,
        state->in_emergency_mode,
        TRACKER_ID,
        state->counter
    );

    return true;
}

bool send_ping(uint8_t* ping_pkt_payload, size_t len) {

    iolist_t iov = {
        .iol_base = ping_pkt_payload,
        .iol_len  = len,
        .iol_next = NULL
    };

    int res = lora_write(&iov);

    if (res < 0) {
        printf("TX FAILED (Error: %d)\n", res);
        return false;
    }

    return true;
}

uint32_t random_between(uint32_t lower, uint32_t upper) {
    if (lower == upper) {
        return lower;
    }
    return random_uint32() % (upper - lower) + lower;
}

uint8_t pack_battery_level(int32_t battery_mv) {
    // 3.3V or higher = Level 7 (100% Full)
    if (battery_mv >= 3300) {
        return 7;
    }
    // 3.1V to 3.29V = Level 6
    if (battery_mv >= 3100) {
        return 6;
    }
    // 2.9V to 3.09V = Level 5
    if (battery_mv >= 2900) {
        return 5;
    }
    // 2.7V to 2.89V = Level 4
    if (battery_mv >= 2700) {
        return 4;
    }
    // 2.5V to 2.69V = Level 3
    if (battery_mv >= 2500) {
        return 3;
    }
    // 2.3V to 2.49V = Level 2
    if (battery_mv >= 2300) {
        return 2;
    }
    // 2.1V to 2.29V = Level 1 (Critically Low - Time to replace!)
    if (battery_mv >= 2100) {
        return 1;
    }
    
    // Anything below 2.1V = Level 0 (Dead / Brown-out imminent)
    return 0;
}

/**
 * @brief This is called by the acme_lora driver IN THE BACKGROUND THREAD 
 * whenever NETDEV_EVENT_RX_COMPLETE fires.
 */
void tracker_lora_rx_cb(void *buffer, size_t len, int16_t *rssi, int8_t *snr) {
    (void)rssi; // Ignore for now, or save them if Trucks Control wants signal stats
    (void)snr;

    if (len <= sizeof(_rx_buffer) && _tracker_thread_pid != KERNEL_PID_UNDEF) {
        // 1. Safely copy the packet out of the driver's memory
        memcpy(_rx_buffer, buffer, len);
        _rx_len = len;
        
        // 2. Shoot a message to wake up the sleeping tracker thread
        msg_t msg;
        msg.type = MSG_TYPE_RX_DONE;
        msg_send(&msg, _tracker_thread_pid);
    }
}

/**
 * @brief Puts the CPU to sleep and waits for a single packet.
 * * @param timeout_ms Maximum time to sleep before giving up.
 * @return length of the packet, or -1 if we timed out.
 */
int wait_for_pong(uint32_t timeout_ms) {
    // 1. Save our current Thread ID so the callback knows who to wake up
    _tracker_thread_pid = thread_getpid(); 
    _rx_len = 0;
    
    // 2. Command the Semtech radio to do a Single RX 
    lora_listen(); 
    
    msg_t msg;
    
    // 3. THE SLEEP COMMAND
    // The OS scheduler pauses this thread right here and turns off the SAM R34 CPU.
    int res = ztimer_msg_receive_timeout(ZTIMER_MSEC, &msg, timeout_ms);
    
    // --- CPU WAKES UP HERE ---
    
    // 4. Force the radio back to sleep mode immediately to save battery
    lora_off();
    
    // 5. Check why we woke up
    if (res < 0) {
        // We timed out. The Trucks Control unit never replied.
        return -1; 
    }
    
    if (msg.type == MSG_TYPE_RX_DONE) {
        // We got a packet! 
        return (int)_rx_len;
    }
    
    return -1;
}