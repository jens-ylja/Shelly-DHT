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

With this, we can decouple the DHT sensors abilities from the Shelly's requirements.
If this approach is working, we can think about supporting DHT11 sensors too. These have a slightly different protocol than the DHT22 ones and are unsupported by Shelly.

### Known Limitations
- Of cource, even if the microcontroller answers the Shelly's requests every 2s, the resolution drops to the least common multiple of the both sides sample intervals. This is known and acceptable.
- The sample intervals of both sides have to be synchronized in a way that they never collide. Since the Shelly's request interval is fixed, the query interval of the DHT has to be dynamically adapted in case a clash becomes reasonable. So the effective resolution of the published meter values is not completely fixed. It may vary by several 100ms.

### Implementation hints and decisions
- For the side of the real DHT sensor, a small custom DHT reader is used instead of a standard library such as `robtillaart/DHTStable`.
  The bridge needs integer values in tenths and tightly controlled timing/collision handling. `robtillaart/DHTStable` uses floating-point values and Arduino I/O calls, which add code size and overhead that are undesirable on the ATtiny85
  and especially when down-sizing to 2 or 4 KB ATtiny 1 series microcontrollers.
  The custom implementation also keeps the sensor protocol and timeouts under direct control.
- For the DHT22 simulation side, no library is known. The implemented custom DHT simulator was inspired by [Rob Tillaart's DHT simulator](https://github.com/RobTillaart/DHT_Simulator/tree/master).

### DHT error states and reported values

The bridge reports these values to the Shelly as valid DHT22 frames (including a calculated checksum):

| State | Temperature | Humidity | Behavior |
|---|---:|---:|---|
| Startup / no sensor response (`DHT_TIMEOUT`) | −40.0 °C | 0.0% | Default values before the first successful sensor reading. |
| Timeout (`DHT_TIMEOUT`) | −40.0 °C | 0.0% | Reported after more than `DHT_MAX_FAILED_INTERVALS` consecutive read failures, if the most recent failure was a timeout. |
| Checksum error (`DHT_CHECKSUM`) | −40.0 °C | 0.1% | Reported after more than `DHT_MAX_FAILED_INTERVALS` consecutive read failures, if the most recent failure was a checksum error. |
| Invalid temperature (outside −40.0…80.0 °C) | −40.2 °C | Valid reading | Only the temperature is replaced. |
| Invalid humidity (above 100.0%) | Valid reading | 0.2% | Only the humidity is replaced. |
| Shelly request collides with a sensor read (`DHT_CANCELLED`) | Unchanged | Unchanged | The interrupted read is discarded; the last published frame remains in use. |

Temperature and humidity are validated independently, so a frame may contain both invalid-value markers if both measurements are out of range. The startup default is used until a sensor read succeeds. Read failures share one consecutive-failure counter; each failure updates the remembered cause, and the corresponding error value is published once the count exceeds `DHT_MAX_FAILED_INTERVALS`. A successful reading resets the counter and updates the frame immediately. Shelly-triggered cancellations do not count as sensor failures.

### Technical restrictions
- The microcontroller can be powered from the Shelly Add On as the DHT sensor is. The Shelly Add On provides 3,3V and a maximum of 15mA. The DHT22 draws about 1-2mA. The microcontroller shouldn't draw more than 5mA to safely stay below the limit.
- The microcontroller can be an ATtiny85 or one of the new ATtiny 1 series with 8 Pins.
- The development was done with VS Code and PlatformIO using the `atmelavr` platform and the `arduino` framework for ATtiny85 (`atmelmegaavr` platform for the ATtiny 1 series devices).
- The DHT line protocol has strict timing restrictions which require the ATtiny to run with a clock speed of at least 8MHz to met them.
