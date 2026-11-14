#pragma once

#include <stdint.h>

enum DhtResult : uint8_t {
  DHT_OK,
  DHT_TIMEOUT,
  DHT_CHECKSUM,
  DHT_CANCELLED
};

void dhtReaderInit();
DhtResult dhtRead(int16_t &temp10, uint16_t &hum10,
                  const volatile bool &cancel);
