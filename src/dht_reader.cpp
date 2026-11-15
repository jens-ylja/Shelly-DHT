#include "dht_reader.h"

#include <Arduino.h>
#include <avr/io.h>
#include <util/delay.h>

#include "hw.h"

#ifdef DHT_DEBUG_SERIAL
DhtDebug dhtDebug;
#endif

static bool waitForLevel(bool high, const volatile bool &cancel) {
  const DhtTimeoutClock start = hwSensorClock();
  for (;;) {
    if (cancel) return false;
    if (hwSensorHigh() == high) return true;
    if (hwSensorTimedOut(start)) return false;
  }
}

void dhtReaderInit() {
  hwSensorRelease();
}

DhtResult dhtRead(int16_t &temp10, uint16_t &hum10,
                  const volatile bool &cancel) {
  uint8_t bytes[5] = {};
#ifdef DHT_DEBUG_SERIAL
  dhtDebug = {};
#endif

  if (cancel) return DHT_TIMEOUT;

  hwSensorPullLow();
  _delay_ms(2);
  hwSensorRelease();

  if (!waitForLevel(false, cancel) || !waitForLevel(true, cancel) ||
      !waitForLevel(false, cancel)) {
    return cancel ? DHT_CANCELLED : DHT_TIMEOUT;
  }

  for (uint8_t bitIndex = 0; bitIndex < 40; ++bitIndex) {
    if (!waitForLevel(true, cancel)) {
#ifdef DHT_DEBUG_SERIAL
      dhtDebug.bitsRead = bitIndex;
      for (uint8_t i = 0; i < 5; ++i) dhtDebug.bytes[i] = bytes[i];
#endif
      return cancel ? DHT_CANCELLED : DHT_TIMEOUT;
    }

    const DhtTimeoutClock highStart = hwSensorClock();
    if (!waitForLevel(false, cancel)) {
#ifdef DHT_DEBUG_SERIAL
      dhtDebug.bitsRead = bitIndex;
      for (uint8_t i = 0; i < 5; ++i) dhtDebug.bytes[i] = bytes[i];
#endif
      return cancel ? DHT_CANCELLED : DHT_TIMEOUT;
    }
    const DhtTimeoutClock highTicks = hwSensorElapsed(highStart);

    const uint8_t byteIndex = bitIndex >> 3;
    bytes[byteIndex] <<= 1;
    if (highTicks > DHT_BIT_THRESHOLD_TICKS) {
      bytes[byteIndex] |= 1;
    }
#ifdef DHT_DEBUG_SERIAL
    dhtDebug.highTicks[bitIndex] = highTicks;
    dhtDebug.bitsRead = bitIndex + 1;
    for (uint8_t i = 0; i < 5; ++i) dhtDebug.bytes[i] = bytes[i];
#endif
  }

  if (static_cast<uint8_t>(bytes[0] + bytes[1] + bytes[2] + bytes[3]) != bytes[4]) {
    return DHT_CHECKSUM;
  }

  hum10 = (static_cast<uint16_t>(bytes[0]) << 8) | bytes[1];
  const uint16_t absoluteTemp =
    (static_cast<uint16_t>(bytes[2] & 0x7F) << 8) | bytes[3];
  temp10 = (bytes[2] & 0x80) ? -static_cast<int16_t>(absoluteTemp)
    : static_cast<int16_t>(absoluteTemp);

  return DHT_OK;
}
