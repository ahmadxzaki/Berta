#ifndef TRACKER_H
#define TRACKER_H

#include <stdint.h>
#include <stdbool.h>
#include "compiler_hints.h"

bool tracker_init(void);
bool tracker_wakeup(void);

#endif