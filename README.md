# Shelly & DHT Sensors

## Context
By using the [Shelly Plus Add On](https://www.shelly.com/de/products/shelly-plus-add-on) one can attatch several types of sensors to a Shelly device.
One of the suported sensors is the DHT22 temperature and humidity sensor which is available in different packages.

According to its specification (e.g. https://cdn.sparkfun.com/assets/f/7/d/9/c/DHT22.pdf), the DHT22 sensor supports a collecting period of >= 2s.
The Shelly with its add on requests a sample almost exactly every 2s (every 4th or 5th request is left out). This was verified by using a Logic Analyzer device recording the data line signal over a period of ~20 minutes.

## The Problem
Several of the cheaper DHT22 sensors are not capable to fit the specification. They get out of sync when called every 2s for a longer period of time.

As found out from the Logic Analyzer device recording, a sample DHT22 responded correctly during the 1st 20s of the test.
Afterwards it started to skip every 2nd request and finally stopped answering at all after around 140 seconds.
This experiment was re-done several times with slightly different time ranges of the failure behaviour each. But the final result was the same - the DHT sensor doesn't answer any longer.
The only thing, one can do to get it answering again is a power cycle for the sensor.

For applications really requiring the 2s resolution, the only way to solve seems to be bying a better, more expensive sensor (`Asair` was named as a potential manufacturer in this context).
Nevertheless, for the most applications a resolution of 10s or even more is completely sufficient. So I decided to try out a way to get this working.

## 1st Solution Approach (rejected)
In a 1st approach a small microcontroller was put into the game which suppresses some of the Shelly's requests by pulling the data line down.
With a pull down interval of 3s, the effective sample interval for the DHT sensor changed to 4s. This changed the sensor to work correctly for several hours. But it didn't fully solved the problem.
With a pull down interval of 9s, the effective sample interval for the DHT changed to 10s. The sensor worked over multiple days without crashing. But in this constellation the Shelly reported `null` values for the temperature every several minutes and sometimes also `null` values for the humidity.
So this approach helped a little but isn't a final production grade solution.

## Solution Approach
In this 2nd solution approach, the microcontroller acts as a bridge between the DHT and the Shelly.
On one side it querries the DHT sensor for samples in a configurable interval and chaches the returned values.
On the other side, the controller serves the Shelly's request and returns the cached meter values.

With this, we can decouple the DHT sensors abilities from the Shelly's requirements. The bridge supports DHT22 and DHT11 sensors; the Shelly always receives DHT22-format frames.

### Known Limitations
- Of cource, even if the microcontroller answers the Shelly's requests every 2s, the resolution drops to the least common multiple of the both sides sample intervals. This is known and acceptable.
- The sample intervals of both sides have to be synchronized in a way that they never collide. The DHT read starts 100–1600 ms after a Shelly transaction, measured against the 2s request grid (including skipped requests). If the 10s read interval expires outside this window, the read is delayed by at most about 500ms. When no Shelly request has been seen for more than 10s, the bridge reads without the window.

### Implementation hints and decisions
- For the side of the real DHT sensor, a small custom DHT reader is used instead of a standard library such as `robtillaart/DHTStable`.
  The bridge needs integer values in tenths and tightly controlled timing/collision handling. `robtillaart/DHTStable` uses floating-point values and Arduino I/O calls, which add code size and overhead that are undesirable on the ATtiny85
  and especially when down-sizing to 2 or 4 KB ATtiny 1 series microcontrollers.
  The custom implementation also keeps the sensor protocol and timeouts under direct control.
- Select the real sensor type with `DHT_SENSOR` in `platformio.ini` build flags: `22` for DHT22 or `11` for DHT11. The setting is independent of the processor target.
- For the DHT22 simulation side, no library is known. The implemented custom DHT simulator was inspired by [Rob Tillaart's DHT simulator](https://github.com/RobTillaart/DHT_Simulator/tree/master).

### DHT11 support

Select the real sensor type with `DHT_SENSOR` in the `platformio.ini` build flags: `22` for DHT22 or `11` for DHT11. This setting is independent of the processor target. For DHT11, the bridge uses a 20 ms host start pulse and converts the temperature and humidity bytes to its tenths-based internal values. The nominal DHT11 resolution is 1 °C and 1% RH, and many sensors report zero fractional bytes. However, the DHT11 format includes fractional bytes, and the tested sensor sends nonzero temperature fractions: a logic-analyzer capture showed 0.5 °C, and a further test showed 0.1 °C steps. The bridge preserves these fractions, but fractional readings may depend on the sensor variant and are not guaranteed for all DHT11 sensors. Humidity was observed as an integer in these tests.

The DHT11's specified operating range, where its accuracy tolerances apply, is 0–50 °C and 20–90% RH. These are not enforced as hard validity limits by the bridge; readings outside this range are passed through, but the specified tolerances no longer apply.

### DHT error states and reported values

The bridge reports these values to the Shelly as valid DHT22 frames (including a calculated checksum). The error-indication values in this table are also used when the real sensor is a DHT11, even though those values are outside the DHT11's specified operating range:

| State | Temperature | Humidity | Behavior |
|---|---:|---:|---|
| Startup / no sensor response (`DHT_TIMEOUT`) | −40.0 °C | 0.0% | Default values before the first successful sensor reading. |
| Timeout (`DHT_TIMEOUT`) | −40.0 °C | 0.0% | Reported after more than `DHT_MAX_FAILED_INTERVALS` consecutive read failures, if the most recent failure was a timeout. |
| Checksum error (`DHT_CHECKSUM`) | −40.0 °C | 0.1% | Reported after more than `DHT_MAX_FAILED_INTERVALS` consecutive read failures, if the most recent failure was a checksum error. |
| Invalid temperature (outside −40.0…80.0 °C) | −40.2 °C | Valid reading | Only the temperature is replaced. |
| Invalid humidity (above 100.0%) | Valid reading | 0.2% | Only the humidity is replaced. |
| Shelly request collides with a sensor read (`DHT_CANCELLED`) | Unchanged | Unchanged | The interrupted read is discarded; the last published frame remains in use. |

Temperature and humidity are validated independently, so a frame may contain both invalid-value markers if both measurements are out of range. The startup default is used until a sensor read succeeds. Read failures share one consecutive-failure counter; each failure updates the remembered cause, and the corresponding error value is published once the count exceeds `DHT_MAX_FAILED_INTERVALS`. A successful reading resets the counter and updates the frame immediately. Shelly-triggered cancellations do not count as sensor failures.

### ATtiny85 pin assignment

The table uses the physical pin numbers of the 8-pin ATtiny85 package:

| Physical pin | ATtiny85 pin | Connection | Why this pin is used |
|---:|---|---|---|
| 1 | PB5 / RESET | RESET | Reserved for reset and ISP programming. |
| 2 | PB3 | DHT sensor data | General-purpose GPIO for the real DHT data line; does not use the Shelly interrupt input. |
| 3 | PB4 | Reserved | Kept available for a possible future DHT power-control connection. |
| 4 | GND | Ground | Common ground for the microcontroller, Shelly Add On, and sensor. |
| 5 | PB0 | Unused | — |
| 6 | PB1 | Unused | — |
| 7 | PB2 / INT0 | Shelly data | The Shelly communication needs edge-triggered interrupt handling; PB2 provides INT0 on the ATtiny85. |
| 8 | VCC | Power | Powered from the Shelly Add On supply. |

### ATtiny412 pin assignment

The table uses the physical pin numbers of the 8-pin ATtiny412 package and also lists the corresponding pin numbers on the 14-pin ATtiny814 test device.

| Function | ATtiny pin | ATtiny412 physical pin | ATtiny814 physical pin | Why this pin is used |
|---|---|---:|---:|---|
| Supply | VDD | 1 | 1 | Powered from the Shelly Add On supply. |
| UPDI / RESET | PA0 | 6 | 10 | Reserved for UPDI programming and reset. |
| Shelly data | PA2 | 5 | 12 | Supports the pin-change interrupt used to detect both edges of the Shelly request. |
| DHT sensor data | PA1 | 4 | 11 | General-purpose GPIO for the real DHT data line. |
| Reserved | PA3 | 7 | 13 | Kept available for possible future DHT power control. |
| Unused | PA6 / PA7 | 2 / 3 | 4 / 5 | Left available. |
| Debug UART TX | — | — | 7 (PB2 / USART0 TX) | Temporary ATtiny814 diagnostic output. |
| Debug UART RX | — | — | 6 (PB3 / USART0 RX) | Add a pull-up to VDD; RX is not used by the firmware. |
| Ground | GND | 8 | 14 | Common ground for the microcontroller, Shelly Add On, and sensor. |

The ATtiny814 build provides temporary USART0 diagnostics at 115200 baud, 8N1, on PB2/TX. The output reports startup/heartbeat status and DHT results, received bytes, checksums, failures, and measured pulse durations. This debug functionality is only enabled for the ATtiny814; it is not available in the ATtiny412 or ATtiny85 builds. Keep USART0 on its default pin route; the alternate TX pin conflicts with the Shelly and DHT signals.

### Technical restrictions
- The microcontroller can be powered from the Shelly Add On as the DHT sensor is. The Shelly Add On provides 3,3V and a maximum of 15mA. The DHT22 draws about 1-2mA. The microcontroller shouldn't draw more than 5mA to safely stay below the limit.
- The microcontroller can be an ATtiny85 or an ATtiny 1 series device with 8 pins (the ATtiny412 is the production target; the ATtiny814 is used for testing).
- The development uses VS Code and PlatformIO with the `atmelavr` platform for ATtiny85 and `atmelmegaavr` for ATtiny 1 devices.
- The DHT line protocol has strict timing restrictions and requires a clock speed of at least 8 MHz. At 3.3 V, the ATtiny 1 series supports up to 10 MHz, so these targets use 8 MHz. The ATtiny 1 OSCCFG fuse must be set to select the 16 MHz oscillator; the core then applies its divide-by-two clock prescaler.
- DHT pulse widths are measured directly from hardware timer counters: Timer0 on the ATtiny85 and TCA0 on the ATtiny412/814. On the ATtiny1 targets, TCA0 is taken over for sensor and Shelly-trigger timing while `millis()` remains on TCD0.

### Notes
- The code was created with heavy support of Claude Code. Please don't blame me too much if you don't like the programming style.
- The code was verified as working for both ATtiny85 and ATtiny814 processors and both DHT22 and DHT11 sensors.
- Short term tests (max 20 minutes) were recorded by an [LA5016 Logic Analyzer](https://www.inno-maker.com/product/la5016/) and the wave timing verified for both sides of the communication (incl. sync between them).
- Mid term tests (~1d) have been run with an ATtiny814 paired with the cheap DHT22 (the one referred to in "The Problem" as not capable for direct connection to the Shelly). The test used the default 10s rate for the DHT22.
- The processors have been added completely "naked" without any other surrounding components - except the pull up for the DHT data line (and for the Rx line on ATtiny814 with UART debugging enabled).
