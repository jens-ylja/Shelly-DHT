#pragma once

#include <stdint.h>

#include "dht_reader.h"

#ifdef DHT_DEBUG_SERIAL
void debugInit();
void debugBoot();
void debugHeartbeat(uint32_t nowMs);
void debugDhtResult(DhtResult result, uint16_t failures);
#else
static inline void debugInit() {}
static inline void debugBoot() {}
static inline void debugHeartbeat(uint32_t) {}
static inline void debugDhtResult(DhtResult, uint16_t) {}
#endif
