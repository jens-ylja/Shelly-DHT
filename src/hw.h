#pragma once

#include <avr/io.h>
#include <Arduino.h>

#if defined(__AVR_ATtiny85__)

#define SHELLY_ISR_VECTOR INT0_vect
#define SHELLY_START_DELAY_US 20
#define SHELLY_ACK_LOW_US 80
#define SHELLY_ACK_HIGH_US 80
#define SHELLY_BIT_LOW_US 49
#define SHELLY_BIT_ONE_HIGH_US 68
#define SHELLY_BIT_ZERO_HIGH_US 24
#define SHELLY_END_LOW_US 50

typedef uint8_t DhtTimeoutClock;
typedef uint32_t ShellyClock;
#define DHT_TIMEOUT_LIMIT 20
#define DHT_BIT_THRESHOLD_TICKS 5
#define DHT_TIMER_TICK_US 8

static inline void hwShellyPullLow() {
  PORTB &= ~_BV(PB2);
  DDRB |= _BV(PB2);
}

static inline void hwShellyRelease() {
  DDRB &= ~_BV(PB2);
  PORTB &= ~_BV(PB2);
}

static inline bool hwShellyHigh() {
  return (PINB & _BV(PB2)) != 0;
}

static inline void hwShellyIrqInit() {
  MCUCR |= _BV(ISC00);
  GIMSK |= _BV(INT0);
}

static inline void hwShellyIrqDisable() {
  GIMSK &= ~_BV(INT0);
}

static inline void hwShellyIrqClearEnable() {
  GIFR = _BV(INTF0);
  GIMSK |= _BV(INT0);
}

static inline void hwShellyIrqAcknowledge() {}

static inline void hwSensorPullLow() {
  PORTB &= ~_BV(PB3);
  DDRB |= _BV(PB3);
}

static inline void hwSensorRelease() {
  DDRB &= ~_BV(PB3);
  PORTB &= ~_BV(PB3);
}

static inline bool hwSensorHigh() {
  return (PINB & _BV(PB3)) != 0;
}

static inline DhtTimeoutClock hwSensorClock() {
  return TCNT0;
}

static inline bool hwSensorTimedOut(DhtTimeoutClock start) {
  return static_cast<uint8_t>(TCNT0 - start) >= DHT_TIMEOUT_LIMIT;
}

static inline DhtTimeoutClock hwSensorElapsed(DhtTimeoutClock start) {
  return static_cast<uint8_t>(TCNT0 - start);
}

static inline void hwTimerInit() {}

static inline ShellyClock hwShellyClock() {
  return micros();
}

static inline uint32_t hwShellyElapsed(ShellyClock start, ShellyClock end) {
  return end - start;
}

#elif defined(__AVR_ATtiny412__) || defined(__AVR_ATtiny814__)

#define SHELLY_ISR_VECTOR PORTA_PORT_vect
#define SHELLY_START_DELAY_US 20
#define SHELLY_ACK_LOW_US 80
#define SHELLY_ACK_HIGH_US 80
#define SHELLY_BIT_LOW_US 49
#define SHELLY_BIT_ONE_HIGH_US 68
#define SHELLY_BIT_ZERO_HIGH_US 24
#define SHELLY_END_LOW_US 50

typedef uint16_t DhtTimeoutClock;
typedef uint16_t ShellyClock;
#define DHT_TIMEOUT_LIMIT 160
#define DHT_BIT_THRESHOLD_TICKS 40
#define DHT_TIMER_TICK_US 1

static inline void hwShellyPullLow() {
  VPORTA.OUT &= ~_BV(2);
  VPORTA.DIR |= _BV(2);
}

static inline void hwShellyRelease() {
  VPORTA.DIR &= ~_BV(2);
  VPORTA.OUT &= ~_BV(2);
}

static inline bool hwShellyHigh() {
  return (VPORTA.IN & _BV(2)) != 0;
}

static inline void hwShellyIrqInit() {
  PORTA.PIN2CTRL = PORT_ISC_BOTHEDGES_gc;
}

static inline void hwShellyIrqDisable() {
  PORTA.PIN2CTRL = PORT_ISC_INTDISABLE_gc;
}

static inline void hwShellyIrqClearEnable() {
  VPORTA.INTFLAGS = _BV(2);
  PORTA.PIN2CTRL = PORT_ISC_BOTHEDGES_gc;
}

static inline void hwShellyIrqAcknowledge() {
  VPORTA.INTFLAGS = _BV(2);
}

static inline void hwSensorPullLow() {
  VPORTA.OUT &= ~_BV(1);
  VPORTA.DIR |= _BV(1);
}

static inline void hwSensorRelease() {
  VPORTA.DIR &= ~_BV(1);
  VPORTA.OUT &= ~_BV(1);
}

static inline bool hwSensorHigh() {
  return (VPORTA.IN & _BV(1)) != 0;
}

static inline DhtTimeoutClock hwSensorClock() {
  return TCA0.SINGLE.CNT;
}

static inline bool hwSensorTimedOut(DhtTimeoutClock start) {
  return static_cast<uint16_t>(TCA0.SINGLE.CNT - start) >= DHT_TIMEOUT_LIMIT;
}

static inline DhtTimeoutClock hwSensorElapsed(DhtTimeoutClock start) {
  return static_cast<uint16_t>(TCA0.SINGLE.CNT - start);
}

static inline void hwTimerInit() {
  takeOverTCA0();
  TCA0.SINGLE.CTRLD = 0;
  TCA0.SINGLE.CTRLB = 0;
  TCA0.SINGLE.CTRLC = 0;
  TCA0.SINGLE.INTCTRL = 0;
  TCA0.SINGLE.PER = 0xFFFF;
  TCA0.SINGLE.CNT = 0;
  TCA0.SINGLE.CTRLA = TCA_SINGLE_CLKSEL_DIV8_gc | TCA_SINGLE_ENABLE_bm;
}

static inline ShellyClock hwShellyClock() {
  return TCA0.SINGLE.CNT;
}

static inline uint32_t hwShellyElapsed(ShellyClock start, ShellyClock end) {
  return static_cast<uint16_t>(end - start);
}

#else
#error "Unsupported microcontroller"
#endif
