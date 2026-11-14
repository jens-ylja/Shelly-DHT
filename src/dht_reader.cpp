#include "dht_reader.h"

#include <Arduino.h>
#include <avr/io.h>
#include <util/delay.h>

#define DHT_SENSOR_PIN PB3
#define DHT_SENSOR_MASK _BV(DHT_SENSOR_PIN)
#define DHT_TIMEOUT_TICKS 20

static inline bool sensorLineHigh() {
  return (PINB & DHT_SENSOR_MASK) != 0;
}

static bool waitForLevel(bool high, const volatile bool &cancel) {
  const uint8_t start = TCNT0;
  for (;;) {
    if (cancel) return false;
    if (sensorLineHigh() == high) return true;
    if (static_cast<uint8_t>(TCNT0 - start) >= DHT_TIMEOUT_TICKS) {
      return false;
    }
  }
}

void dhtReaderInit() {
  DDRB &= ~DHT_SENSOR_MASK;
  PORTB &= ~DHT_SENSOR_MASK;
}

DhtResult dhtRead(int16_t &temp10, uint16_t &hum10,
                  const volatile bool &cancel) {
  uint8_t bytes[5] = {};

  if (cancel) return DHT_TIMEOUT;

  PORTB &= ~DHT_SENSOR_MASK;
  DDRB |= DHT_SENSOR_MASK;
  _delay_ms(1);
  DDRB &= ~DHT_SENSOR_MASK;

  if (!waitForLevel(false, cancel) || !waitForLevel(true, cancel) ||
      !waitForLevel(false, cancel)) {
    return cancel ? DHT_CANCELLED : DHT_TIMEOUT;
  }

  for (uint8_t bitIndex = 0; bitIndex < 40; ++bitIndex) {
    if (!waitForLevel(true, cancel)) {
      return cancel ? DHT_CANCELLED : DHT_TIMEOUT;
    }

    const uint32_t highStart = micros();
    if (!waitForLevel(false, cancel)) {
      return cancel ? DHT_CANCELLED : DHT_TIMEOUT;
    }
    const uint32_t highTime = micros() - highStart;

    const uint8_t byteIndex = bitIndex >> 3;
    bytes[byteIndex] <<= 1;
    if (highTime > 40UL) {
      bytes[byteIndex] |= 1;
    }
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
