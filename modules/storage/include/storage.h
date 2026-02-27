#ifndef STORAGE_H
#define STORAGE_H

#include <stdint.h>
#include <stdbool.h>

#include "tracker.h"

typedef struct {
    bool in_emergency_mode;
    uint32_t counter;
    uint8_t missed_truck_reply_count;
} tracker_state_t;

bool storage_init(void);

bool backup_state(tracker_state_t state);

bool load_state(tracker_state_t *stateOut);

#endif