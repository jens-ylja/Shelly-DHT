#include <Arduino.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#include "dht_reader.h"
#include "hw.h"

#ifdef DHT_DEBUG_SERIAL
#ifndef DHT_DEBUG_BAUD
#define DHT_DEBUG_BAUD 115200
#endif
#endif

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

static volatile ShellyClock lowStartTicks = 0;
static volatile uint32_t lowStartMs = 0;
static uint8_t frame[5];

#ifdef DHT_DEBUG_SERIAL
static void printHexByte(uint8_t value) {
  if (value < 0x10) Serial.print('0');
  Serial.print(value, HEX);
}

static void printDhtDebug(DhtResult result, uint16_t failures) {
  Serial.print(F("R="));
  switch (result) {
    case DHT_OK: Serial.print(F("OK")); break;
    case DHT_TIMEOUT: Serial.print(F("TIMEOUT")); break;
    case DHT_CHECKSUM: Serial.print(F("CHECKSUM")); break;
    case DHT_CANCELLED: Serial.print(F("CANCELLED")); break;
  }
  Serial.print(F(" bits="));
  Serial.print(dhtDebug.bitsRead);
  Serial.print(F(" b="));
  for (uint8_t i = 0; i < 5; ++i) {
    if (i) Serial.write(' ');
    printHexByte(dhtDebug.bytes[i]);
  }
  Serial.print(F(" sum="));
  printHexByte(static_cast<uint8_t>(dhtDebug.bytes[0] + dhtDebug.bytes[1] +
                                    dhtDebug.bytes[2] + dhtDebug.bytes[3]));
  Serial.print(F(" fail="));
  Serial.println(failures);

  Serial.print(F("t_us="));
  for (uint8_t i = 0; i < dhtDebug.bitsRead; ++i) {
    if (i) Serial.write(',');
    Serial.print(static_cast<uint16_t>(dhtDebug.highTicks[i]) * DHT_TIMER_TICK_US);
  }
  Serial.println();
}
#endif

static inline void pullDataLow() {
  hwShellyPullLow();
}

static inline void releaseDataLine() {
  hwShellyRelease();
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
    _delay_us(SHELLY_BIT_LOW_US);
    releaseDataLine();
    if (value & mask) {
      _delay_us(SHELLY_BIT_ONE_HIGH_US);
    } else {
      _delay_us(SHELLY_BIT_ZERO_HIGH_US);
    }
  }
}

static void sendResponse() {
  const uint8_t savedSreg = SREG;
  cli();

  hwShellyIrqDisable();
  _delay_us(SHELLY_START_DELAY_US);
  pullDataLow();
  _delay_us(SHELLY_ACK_LOW_US);
  releaseDataLine();
  _delay_us(SHELLY_ACK_HIGH_US);

  for (uint8_t i = 0; i < 5; ++i) {
    sendByte(frame[i]);
  }

  pullDataLow();
  _delay_us(SHELLY_END_LOW_US);
  releaseDataLine();

  hwShellyIrqAcknowledge();
  hwShellyIrqClearEnable();
  shellyBusy = false;
  SREG = savedSreg;
}

ISR(SHELLY_ISR_VECTOR) {
  hwShellyIrqAcknowledge();
  if (!hwShellyHigh()) {
    lowStartTicks = hwShellyClock();
    lowStartMs = millis();
    shellyBusy = true;
    shellyCollision = true;
    return;
  }

  const ShellyClock lowEndTicks = hwShellyClock();
  const uint32_t lowEndMs = millis();
  const uint32_t elapsedMs = lowEndMs - lowStartMs;
  const uint32_t lowDuration = hwShellyElapsed(lowStartTicks, lowEndTicks);
  if (elapsedMs <= DHT_TRIGGER_MAX_US / 1000UL + 1UL &&
      lowDuration >= DHT_TRIGGER_MIN_US && lowDuration <= DHT_TRIGGER_MAX_US) {
    lastTriggerMs = lowEndMs;
    sendResponse();
  } else {
    shellyBusy = false;
  }
}

void setup() {
  hwTimerInit();
#ifdef DHT_DEBUG_SERIAL
  Serial.begin(DHT_DEBUG_BAUD, SERIAL_TX_ONLY | SERIAL_8N1);
#endif
  releaseDataLine();
  dhtReaderInit();
#ifdef DHT_DEBUG_SERIAL
  Serial.println(F("BOOT"));
  Serial.println(hwShellyHigh() ? F("PA2=1") : F("PA2=0"));
  Serial.flush();
#endif
  publishFrame(DHT_DEFAULT_TEMP10, DHT_DEFAULT_HUM10);

  hwShellyIrqInit();
  sei();
}

void loop() {
  static uint32_t lastDhtAttemptMs = 0;
#ifdef DHT_DEBUG_SERIAL
  static uint32_t lastDebugHeartbeatMs = 0;
#endif
  static uint16_t consecutiveFailures = 0;
  static DhtResult lastFailure = DHT_TIMEOUT;

  const uint32_t now = millis();
#ifdef DHT_DEBUG_SERIAL
  if (now - lastDebugHeartbeatMs >= 1000UL) {
    lastDebugHeartbeatMs = now;
    Serial.print(F("HB ms="));
    Serial.print(now);
    Serial.print(F(" PA2="));
    Serial.print(hwShellyHigh() ? '1' : '0');
    Serial.print(F(" busy="));
    Serial.println(shellyBusy ? '1' : '0');
  }
#endif
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

#ifdef DHT_DEBUG_SERIAL
  const uint16_t debugFailures = result == DHT_OK ? 0 :
    (consecutiveFailures < 65535 ? consecutiveFailures + 1 : consecutiveFailures);
  printDhtDebug(result, debugFailures);
#endif

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
