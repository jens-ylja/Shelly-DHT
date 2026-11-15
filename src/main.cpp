#include <Arduino.h>
#include <avr/interrupt.h>

#include "debug.h"
#include "dht_reader.h"
#include "dht_simulator.h"
#include "hw.h"

#ifndef DHT_READ_INTERVAL_MS
#error "DHT_READ_INTERVAL_MS must be set in platformio.ini build_flags"
#endif
#ifndef DHT_MAX_FAILED_INTERVALS
#error "DHT_MAX_FAILED_INTERVALS must be set in platformio.ini build_flags"
#endif

void setup() {
  hwTimerInit();
  debugInit();
  dhtReaderInit();
  debugBoot();
  dhtSimulatorInit();
  sei();
}

void loop() {
  static uint32_t lastDhtAttemptMs = 0;
  static uint16_t consecutiveFailures = 0;
  static DhtResult lastFailure = DHT_TIMEOUT;

  const uint32_t now = millis();
  debugHeartbeat(now);
  if (now - lastDhtAttemptMs < DHT_READ_INTERVAL_MS ||
      !dhtSimulatorClaimReadWindow(now)) return;

  lastDhtAttemptMs = now;
  int16_t temp10;
  uint16_t hum10;
  const DhtResult result = dhtRead(temp10, hum10, dhtSimulatorCollision());

  if (dhtSimulatorTakeCollision() || result == DHT_CANCELLED) return;

  if (result == DHT_OK) {
    consecutiveFailures = 0;
    debugDhtResult(result, consecutiveFailures);
    dhtSimulatorPublish(temp10, hum10);
    return;
  }

  lastFailure = result;
  if (consecutiveFailures < 65535) ++consecutiveFailures;
  debugDhtResult(result, consecutiveFailures);
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
