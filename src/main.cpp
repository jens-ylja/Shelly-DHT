#include <Arduino.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#define DHT_DATA_PIN PB2
#define DHT_DATA_MASK _BV(DHT_DATA_PIN)

// Values are represented in 0.1 units (e.g. 23.5 C = 235).
volatile int16_t temperature = 235;
volatile uint16_t humidity = 456;

static inline void pullDataLow() {
  PORTB &= ~DHT_DATA_MASK;
  DDRB |= DHT_DATA_MASK;
}

static inline void releaseDataLine() {
  DDRB &= ~DHT_DATA_MASK;
  PORTB &= ~DHT_DATA_MASK;
}

static void buildFrame(uint8_t frame[5]) {
  int16_t temp;
  uint16_t rh;

  // Keep the 16-bit reads consistent if values are updated asynchronously.
  const uint8_t savedSreg = SREG;
  cli();
  temp = temperature;
  rh = humidity;
  SREG = savedSreg;

  if (temp < -400) temp = -400;
  if (temp > 800) temp = 800;
  if (rh > 1000) rh = 1000;

  frame[0] = static_cast<uint8_t>(rh >> 8);
  frame[1] = static_cast<uint8_t>(rh);

  const bool negative = temp < 0;
  const uint16_t absoluteTemp = negative ? static_cast<uint16_t>(-temp)
    : static_cast<uint16_t>(temp);
  frame[2] = static_cast<uint8_t>(absoluteTemp >> 8);
  if (negative) frame[2] |= 0x80;
  frame[3] = static_cast<uint8_t>(absoluteTemp);
  frame[4] = static_cast<uint8_t>(frame[0] + frame[1] + frame[2] + frame[3]);
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

static void sendResponse(const uint8_t frame[5]) {
  // Preserve any interrupt state that was in effect before transmission.
  const uint8_t savedSreg = SREG;
  cli();

  _delay_us(30);
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
  SREG = savedSreg;
}

void setup() {
  // DHT single-wire signaling is open-drain: only pull low or release.
  releaseDataLine();
}

void loop() {
  if (PINB & DHT_DATA_MASK) {
    return;
  }

  const uint32_t start = micros();
  uint8_t frame[5];
  buildFrame(frame);

  while (!(PINB & DHT_DATA_MASK)) {
    if (micros() - start > 20000UL) {
      return;
    }
  }

  const uint32_t lowDuration = micros() - start;
  if (lowDuration >= 500UL) {
    sendResponse(frame);
  }
}
