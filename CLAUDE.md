# Shelly & DHT Sensors

## Context
By using the [Shelly Plus Add On](https://www.shelly.com/de/products/shelly-plus-add-on) one can attatch several types of sensors to a Shelly device.
One of the suported sensors is the DHT22 temperature and humidity sensor which is available in different packages.

According to its specification (e.g. https://cdn.sparkfun.com/assets/f/7/d/9/c/DHT22.pdf), the DHT22 sensor supports a collecting period of >= 2s.
The Shelly with its add on requests a sample almost exactly every 2s (every 4th or 5th request is left out). This was verified by using a Logic Analyzer device recording the data line signal over a period of ~20 minutes.

## The Problem
Several of the cheaper DHT22 sensors are not capable to fit the specification. They get out of sync when called every 2s for a longer period of time.

As found out from the Logic Analyzer device recording (see Shelly-DHT22.xlsx), the DHT22 responded correctly during the 1st 20s of the test.
Afterwards it started to skip every 2nd request and finally stopped answering at all after around 138 seconds.
This experiment was re-done several times with slightly different time ranges each but the final result was the same - the DHT sensor doesn't answer any longer.
The only thing, one can do to get it answering again is a power cycle for the sensor.

## 1st Solution Approach (rejected)
In a 1st approach a small microcontroller was put into the game which suppresses some of the Shelly's requests by pulling the data line down.
With a pull down interval of 3s, the effective sample interval for the DHT sensor changed to 4s which changed the sensor to work correctly for several hours. But it didn't fully solved the problem.
With a pull down interval of 9s, the effective sample interval for the DHT changed to 10s. The sensor worked over multiple days without crashing. But in this constellation the Shelly reported `null` values for the temperature every several minutes and sometimes also `null` values for the humidity.
So this approach helped a little but isn't a final solution.

## Solution Approach
In this 2nd solution approach, the microcontroller should act as a bridge between the DHT and the Shelly.
On one side it should querry the DHT sensor for samples in a configurable interval and chache the returned values.
On the other side the controller should serve the Shelly's request and return the cached value.

With this we can decouple the DHT sensors ability from the Shelly's requirements.
If this approach is working, we can think about supporting DHT11 sensors too. These have a slightly different protocol than the DHT11 ones and are unsupported by Shelly.

### Known Limitations
- Of cource, even it the microcontroller answers the Shelly's requests every 2s, the resolution drops to the least common multiple of the both sides sample interval. This is known and acceptable.
- The sample intervals of both sides have to be synchronized in a way that they never collide. Since the Shelly's request interval is fixed, the query interval of the DHT has to be dynamically adapted in case a clash becomes reasonable.

### Technical restrictions
- The microcontroller has to be powered from the Shelly Add On as the DHT sensor is. The Shelly Add On provides 3,3V and a maximum of 15mA. The DHT22 draws about 1-2mA. The microcontroller shouldn't draw more than 5mA to safely stay below the limit.
- The microcontroller should be an ATtiny85 (for the prototype phase) or later on one of the new ATtiny 0 or 1 series with 8 Pins.
- The development has to be done with VS Code and PlatformIO using the `atmelavr` platform and the `arduino` framework for ATtiny85 (`atmelmegaavr` platform for the ATtiny 0/1 series devices).
- For accessing the DHT sensor there are libraries available (e.g. `robtillaart/DHTStable`). Maybe they can be used, maybe not.
- For the DHT22 simulation side, no library is known to me.
- The DHT line protocol has strict timing restrictions which require the ATtiny to run with a clock speed of at least 8MHz to met them.
