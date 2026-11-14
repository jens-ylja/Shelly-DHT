#include <Arduino.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#define DHT_DATA_PIN PB2
#define DHT_DATA_MASK _BV(DHT_DATA_PIN)
#define DHT_TRIGGER_MIN_US 500UL
#define DHT_TRIGGER_MAX_US 20000UL

// Values are represented in 0.1 units (e.g. 23.5 C = 235).
volatile int16_t temperature = 235;
volatile uint16_t humidity = 456;

volatile uint32_t lastTriggerMs = 0;
volatile bool shellyBusy = false;

static volatile uint32_t lowStartUs = 0;
static volatile bool frameReady = false;
static uint8_t frame[5];

static inline void pullDataLow() {
  PORTB &= ~DHT_DATA_MASK;
  DDRB |= DHT_DATA_MASK;
}

static inline void releaseDataLine() {
  DDRB &= ~DHT_DATA_MASK;
  PORTB &= ~DHT_DATA_MASK;
}

static void buildFrame(uint8_t output[5]) {
  int16_t temp = temperature;
  uint16_t rh = humidity;

  if (temp < -400) temp = -400;
  if (temp > 800) temp = 800;
  if (rh > 1000) rh = 1000;

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

  // Disable INT0 while the MCU drives the data line itself.
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
    buildFrame(frame);
    frameReady = true;
    shellyBusy = true;
    return;
  }

  const uint32_t lowDuration = micros() - lowStartUs;
  if (frameReady && lowDuration >= DHT_TRIGGER_MIN_US &&
      lowDuration <= DHT_TRIGGER_MAX_US) {
    lastTriggerMs = millis();
    frameReady = false;
    sendResponse();
  } else {
    frameReady = false;
    shellyBusy = false;
  }
}

void setup() {
  releaseDataLine();

  // INT0 on PB2, trigger on both falling and rising edges.
  MCUCR |= _BV(ISC00);
  GIMSK |= _BV(INT0);
  sei();
}

void loop() {
}
