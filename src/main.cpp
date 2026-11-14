#include <Arduino.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#include "dht_reader.h"

#define DHT_DATA_PIN PB2
#define DHT_DATA_MASK _BV(DHT_DATA_PIN)
#define DHT_TRIGGER_MIN_US 500UL
#define DHT_TRIGGER_MAX_US 20000UL
#ifndef DHT_READ_INTERVAL_MS
#error "DHT_READ_INTERVAL_MS must be set in platformio.ini build_flags"
#endif
#ifndef DHT_MAX_FAILED_INTERVALS
#error "DHT_MAX_FAILED_INTERVALS must be set in platformio.ini build_flags"
#endif
#ifndef DHT_SAFE_DELAY_AFTER_SHELLY_MS
#define DHT_SAFE_DELAY_AFTER_SHELLY_MS 300UL
#endif
#define DHT_DEFAULT_TEMP10 -400
#define DHT_DEFAULT_HUM10 0
#define DHT_INVALID_TEMP10 -402
#define DHT_CHECKSUM_ERROR_HUM10 1
#define DHT_INVALID_HUM10 2

volatile uint32_t lastTriggerMs = 0;
volatile bool shellyBusy = false;
volatile bool shellyCollision = false;

static volatile uint32_t lowStartUs = 0;
static uint8_t frame[5];

static inline void pullDataLow() {
  PORTB &= ~DHT_DATA_MASK;
  DDRB |= DHT_DATA_MASK;
}

static inline void releaseDataLine() {
  DDRB &= ~DHT_DATA_MASK;
  PORTB &= ~DHT_DATA_MASK;
}

static void buildFrame(int16_t temp, uint16_t rh, uint8_t output[5]) {
  if (temp < -400 || temp > 800) temp = DHT_INVALID_TEMP10;
  if (rh > 1000) rh = DHT_INVALID_HUM10;

  output[0] = static_cast<uint8_t>(rh >> 8);
  output[1] = static_cast<uint8_t>(rh);

  const bool negative = temp < 0;
  const uint16_t absoluteTemp = negative ? static_cast<uint16_t>(-temp)
    : static_cast<uint16_t>(temp);
  output[2] = static_cast<uint8_t>(absoluteTemp >> 8);
  if (negative) output[2] |= 0x80;
  output[3] = static_cast<uint8_t>(absoluteTemp);
  output[4] = static_cast<uint8_t>(output[0] + output[1] + output[2] + output[3]);
}

static void publishFrame(int16_t temp, uint16_t rh) {
  uint8_t newFrame[5];
  buildFrame(temp, rh, newFrame);

  const uint8_t savedSreg = SREG;
  cli();
  for (uint8_t i = 0; i < 5; ++i) {
    frame[i] = newFrame[i];
  }
  SREG = savedSreg;
}

static void sendByte(uint8_t value) {
  for (uint8_t mask = 0x80; mask != 0; mask >>= 1) {
    pullDataLow();
    _delay_us(49);
    releaseDataLine();
    if (value & mask) {
      _delay_us(68);
    } else {
      _delay_us(24);
    }
  }
}

static void sendResponse() {
  const uint8_t savedSreg = SREG;
  cli();

  GIMSK &= ~_BV(INT0);
  _delay_us(20);
  pullDataLow();
  _delay_us(80);
  releaseDataLine();
  _delay_us(80);

  for (uint8_t i = 0; i < 5; ++i) {
    sendByte(frame[i]);
  }

  pullDataLow();
  _delay_us(50);
  releaseDataLine();

  GIFR = _BV(INTF0);
  GIMSK |= _BV(INT0);
  shellyBusy = false;
  SREG = savedSreg;
}

ISR(INT0_vect) {
  if (!(PINB & DHT_DATA_MASK)) {
    lowStartUs = micros();
    shellyBusy = true;
    shellyCollision = true;
    return;
  }

  const uint32_t lowDuration = micros() - lowStartUs;
  if (lowDuration >= DHT_TRIGGER_MIN_US && lowDuration <= DHT_TRIGGER_MAX_US) {
    lastTriggerMs = millis();
    sendResponse();
  } else {
    shellyBusy = false;
  }
}

void setup() {
  releaseDataLine();
  dhtReaderInit();
  publishFrame(DHT_DEFAULT_TEMP10, DHT_DEFAULT_HUM10);

  MCUCR |= _BV(ISC00);
  GIMSK |= _BV(INT0);
  sei();
}

void loop() {
  static uint32_t lastDhtAttemptMs = 0;
  static uint16_t consecutiveFailures = 0;
  static DhtResult lastFailure = DHT_TIMEOUT;

  const uint32_t now = millis();
  const uint8_t savedSreg = SREG;
  cli();
  const bool readyToRead = !shellyBusy &&
    now - lastDhtAttemptMs >= DHT_READ_INTERVAL_MS &&
    now - lastTriggerMs >= DHT_SAFE_DELAY_AFTER_SHELLY_MS;
  if (readyToRead) {
    shellyCollision = false;
  }
  SREG = savedSreg;

  if (!readyToRead) return;

  lastDhtAttemptMs = now;
  int16_t temp10;
  uint16_t hum10;
  const DhtResult result = dhtRead(temp10, hum10, shellyCollision);

  const uint8_t resultSreg = SREG;
  cli();
  const bool collision = shellyCollision || shellyBusy;
  shellyCollision = false;
  SREG = resultSreg;

  if (collision || result == DHT_CANCELLED) return;

  if (result == DHT_OK) {
    consecutiveFailures = 0;
    publishFrame(temp10, hum10);
    return;
  }

  lastFailure = result;
  if (consecutiveFailures < 65535) ++consecutiveFailures;
  if (consecutiveFailures <= DHT_MAX_FAILED_INTERVALS) return;

  switch (lastFailure) {
    case DHT_TIMEOUT:
      publishFrame(DHT_DEFAULT_TEMP10, DHT_DEFAULT_HUM10);
      break;
    case DHT_CHECKSUM:
      publishFrame(DHT_DEFAULT_TEMP10, DHT_CHECKSUM_ERROR_HUM10);
      break;
    case DHT_OK:
    case DHT_CANCELLED:
      break;
  }
}
