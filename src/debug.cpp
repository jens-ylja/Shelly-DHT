#include "debug.h"

#ifdef DHT_DEBUG_SERIAL
#include <Arduino.h>

#include "dht_simulator.h"
#include "hw.h"

#ifndef DHT_DEBUG_BAUD
#define DHT_DEBUG_BAUD 115200
#endif

static void printHexByte(uint8_t value) {
  if (value < 0x10) Serial.print('0');
  Serial.print(value, HEX);
}

void debugInit() {
  Serial.begin(DHT_DEBUG_BAUD, SERIAL_TX_ONLY | SERIAL_8N1);
}

void debugBoot() {
  Serial.println(F("BOOT"));
  Serial.println(hwShellyHigh() ? F("PA2=1") : F("PA2=0"));
  Serial.flush();
}

void debugHeartbeat(uint32_t nowMs) {
  static uint32_t lastDebugHeartbeatMs = 0;
  if (nowMs - lastDebugHeartbeatMs < 1000UL) return;

  lastDebugHeartbeatMs = nowMs;
  Serial.print(F("HB ms="));
  Serial.print(nowMs);
  Serial.print(F(" PA2="));
  Serial.print(hwShellyHigh() ? '1' : '0');
  Serial.print(F(" busy="));
  Serial.println(dhtSimulatorBusy() ? '1' : '0');
}

void debugDhtResult(DhtResult result, uint16_t failures) {
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
