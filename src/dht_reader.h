#pragma once

#include <stdint.h>

enum DhtResult : uint8_t {
  DHT_OK,
  DHT_TIMEOUT,
  DHT_CHECKSUM,
  DHT_CANCELLED
};

#ifdef DHT_DEBUG_SERIAL
struct DhtDebug {
  uint8_t bytes[5];
  uint8_t bitsRead;
  uint8_t highTicks[40];
};

extern DhtDebug dhtDebug;
#endif

void dhtReaderInit();
DhtResult dhtRead(int16_t &temp10, uint16_t &hum10, const volatile bool &cancel);
