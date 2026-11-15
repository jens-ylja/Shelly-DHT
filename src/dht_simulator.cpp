#include "dht_simulator.h"

#include <Arduino.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#include "hw.h"

#define DHT_TRIGGER_MIN_US 500UL
#define DHT_TRIGGER_MAX_US 20000UL
#ifndef DHT_READ_INTERVAL_MS
#define DHT_READ_INTERVAL_MS 10000UL
#endif
#ifndef DHT_SHELLY_GRID_MS
#define DHT_SHELLY_GRID_MS 2000UL
#endif
#ifndef DHT_READ_WINDOW_START_MS
#define DHT_READ_WINDOW_START_MS 120UL
#endif
#ifndef DHT_READ_WINDOW_END_MS
#define DHT_READ_WINDOW_END_MS 1600UL
#endif
#define DHT_INVALID_TEMP10 -402
#define DHT_INVALID_HUM10 2

static volatile uint32_t lastTriggerMs = 0;
static volatile bool shellySeen = false;
static volatile bool shellyBusy = false;
static volatile bool shellyCollision = false;
static volatile ShellyClock lowStartTicks = 0;
static volatile uint32_t lowStartMs = 0;
static uint8_t frame[5];

static void buildFrame(int16_t temp, uint16_t rh, uint8_t output[5]) {
  if (temp < -400 || temp > 800) {
    temp = DHT_INVALID_TEMP10;
  }
  if (rh > 1000) {
    rh = DHT_INVALID_HUM10;
  }

  output[0] = static_cast<uint8_t>(rh >> 8);
  output[1] = static_cast<uint8_t>(rh);

  const bool negative = temp < 0;
  const uint16_t absoluteTemp = negative ? static_cast<uint16_t>(-temp) : static_cast<uint16_t>(temp);
  output[2] = static_cast<uint8_t>(absoluteTemp >> 8);
  if (negative) {
    output[2] |= 0x80;
  }
  output[3] = static_cast<uint8_t>(absoluteTemp);
  output[4] = static_cast<uint8_t>(output[0] + output[1] + output[2] + output[3]);
}

void dhtSimulatorPublish(int16_t temp10, uint16_t hum10) {
  uint8_t newFrame[5];
  buildFrame(temp10, hum10, newFrame);

  const uint8_t savedSreg = SREG;
  cli();
  for (uint8_t i = 0; i < 5; ++i) {
    frame[i] = newFrame[i];
  }
  SREG = savedSreg;
}

static void sendByte(uint8_t value) {
  for (uint8_t mask = 0x80; mask != 0; mask >>= 1) {
    hwShellyPullLow();
    _delay_us(SHELLY_BIT_LOW_US);
    hwShellyRelease();
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
  hwShellyPullLow();
  _delay_us(SHELLY_ACK_LOW_US);
  hwShellyRelease();
  _delay_us(SHELLY_ACK_HIGH_US);

  for (uint8_t i = 0; i < 5; ++i) {
    sendByte(frame[i]);
  }

  hwShellyPullLow();
  _delay_us(SHELLY_END_LOW_US);
  hwShellyRelease();

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
  if (elapsedMs <= DHT_TRIGGER_MAX_US / 1000UL + 1UL
      && lowDuration >= DHT_TRIGGER_MIN_US
      && lowDuration <= DHT_TRIGGER_MAX_US) {
    sendResponse();
    lastTriggerMs = lowEndMs;
    shellySeen = true;
  } else {
    shellyBusy = false;
  }
}

void dhtSimulatorInit() {
  hwShellyRelease();
  dhtSimulatorPublish(DHT_DEFAULT_TEMP10, DHT_DEFAULT_HUM10);
  hwShellyIrqInit();
}

bool dhtSimulatorClaimReadWindow(uint32_t nowMs) {
  (void)nowMs;
  const uint8_t savedSreg = SREG;
  cli();
  const uint32_t claimTimeMs = millis();
  const uint32_t sinceShellyMs = claimTimeMs - lastTriggerMs;
  bool inReadWindow = !shellySeen || sinceShellyMs >= DHT_READ_INTERVAL_MS;
  if (shellySeen && !inReadWindow) {
    uint32_t phaseMs = sinceShellyMs;
    while (phaseMs >= DHT_SHELLY_GRID_MS) {
      phaseMs -= DHT_SHELLY_GRID_MS;
    }
    inReadWindow = phaseMs >= DHT_READ_WINDOW_START_MS && phaseMs <= DHT_READ_WINDOW_END_MS;
  }
  const bool ready = !shellyBusy && inReadWindow;
  if (ready) {
    shellyCollision = false;
  }
  SREG = savedSreg;
  return ready;
}

const volatile bool& dhtSimulatorCollision() {
  return shellyCollision;
}

bool dhtSimulatorTakeCollision() {
  const uint8_t savedSreg = SREG;
  cli();
  const bool collision = shellyCollision || shellyBusy;
  shellyCollision = false;
  SREG = savedSreg;
  return collision;
}

bool dhtSimulatorBusy() {
  return shellyBusy;
}
