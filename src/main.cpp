#include <Arduino.h>
#include <avr/interrupt.h>

#include "dht_reader.h"
#include "dht_simulator.h"
#include "hw.h"

#ifdef DHT_DEBUG_SERIAL
#ifndef DHT_DEBUG_BAUD
#define DHT_DEBUG_BAUD 115200
#endif
#endif

#ifndef DHT_READ_INTERVAL_MS
#error "DHT_READ_INTERVAL_MS must be set in platformio.ini build_flags"
#endif
#ifndef DHT_MAX_FAILED_INTERVALS
#error "DHT_MAX_FAILED_INTERVALS must be set in platformio.ini build_flags"
#endif

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

void setup() {
  hwTimerInit();
#ifdef DHT_DEBUG_SERIAL
  Serial.begin(DHT_DEBUG_BAUD, SERIAL_TX_ONLY | SERIAL_8N1);
#endif
  dhtReaderInit();
#ifdef DHT_DEBUG_SERIAL
  Serial.println(F("BOOT"));
  Serial.println(hwShellyHigh() ? F("PA2=1") : F("PA2=0"));
  Serial.flush();
#endif
  dhtSimulatorInit();
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
    Serial.println(dhtSimulatorBusy() ? '1' : '0');
  }
#endif
  if (now - lastDhtAttemptMs < DHT_READ_INTERVAL_MS ||
      !dhtSimulatorClaimReadWindow(now)) return;

  lastDhtAttemptMs = now;
  int16_t temp10;
  uint16_t hum10;
  const DhtResult result = dhtRead(temp10, hum10, dhtSimulatorCollision());

  if (dhtSimulatorTakeCollision() || result == DHT_CANCELLED) return;

#ifdef DHT_DEBUG_SERIAL
  const uint16_t debugFailures = result == DHT_OK ? 0 :
    (consecutiveFailures < 65535 ? consecutiveFailures + 1 : consecutiveFailures);
  printDhtDebug(result, debugFailures);
#endif

  if (result == DHT_OK) {
    consecutiveFailures = 0;
    dhtSimulatorPublish(temp10, hum10);
    return;
  }

  lastFailure = result;
  if (consecutiveFailures < 65535) ++consecutiveFailures;
  if (consecutiveFailures <= DHT_MAX_FAILED_INTERVALS) return;

  switch (lastFailure) {
    case DHT_TIMEOUT:
      dhtSimulatorPublish(DHT_DEFAULT_TEMP10, DHT_DEFAULT_HUM10);
      break;
    case DHT_CHECKSUM:
      dhtSimulatorPublish(DHT_DEFAULT_TEMP10, DHT_CHECKSUM_ERROR_HUM10);
      break;
    case DHT_OK:
    case DHT_CANCELLED:
      break;
  }
}
