# lighting-arduino

## Background

People have various preferences when it comes to their physical work
environment, and it's common for these preferences to conflict. Unfortunately,
in the matter of lighting, it's common for this conflict to manifest in the
form of a war over the dimmer-switch, leading to fluctuations in lighting
throughout the day -- which tends to give me a headache. 

`lighting-arduino` seeks to use the [`SparkFun ESP32 Thing
Plus`][sparkfun-thing-plus] and the [`SparkFun Ambient Light
Sensor`][sparkfun-light-sensor] to measure and track workspace illuminance over
time.

OSHA [specifies a minimum illuminance of 30 foot-candles][osha-general-office]
in general office settings, with a [recommendation of 20-50
foot-candles][osha-monitors] when working with CRT monitors, and up to 73
foot-candles when working with LED monitors (no minimum is set forth in this
case).

[sparkfun-thing-plus]:https://www.sparkfun.com/products/14689
[sparkfun-light-sensor]:https://www.sparkfun.com/products/15436
[osha-general-office]:https://www.osha.gov/laws-regs/regulations/standardnumber/1926/1926.56
[osha-monitors]:https://www.osha.gov/SLTC/etools/computerworkstations/wkstation_enviro.html

## Reading

- [SparkFun Thing Plus][sparkfun-thing-plus]
- [Serial comm on Linux][linux-serial-comm]
- [Info about BLE and gatttool][gatttool]

[linux-serial-comm]:https://www.cyberciti.biz/hardware/5-linux-unix-commands-for-connecting-to-the-serial-console/
[sparkfun-thing-plus]:https://learn.sparkfun.com/tutorials/esp32-thing-plus-hookup-guide
[gatttool]:https://www.jaredwolff.com/get-started-with-bluetooth-low-energy/

## The plan

- [x] Serial output of the current illuminance, with support for configurable
  linear backoff polling to conserve energy
- [x] Configuration for "acceptable min/max illuminance" and "LED behavior",
  s.t. the on-board LED either turns on, turns off, or blinks when the light
  level is acceptable (or unacceptable)
- [x] BLE server w/ real-time output
- [ ] Storage of metrics (one datapoint at variable rates based on recency --
  every sec for the last min, and every 15min after that) long-term trends
- [ ] (Stretch goal) a way to output a graph from the metrics


## Behavior

### Bluetooth

This sketch exposes a BLE peripheral (server) that sends notifications each
containing a field of the current datapoint and comprising a pair of 4-byte
integers of the format `<FIELD_ID> <FIELD_VALUE>`. Much is written about BLE,
but it's hard to find a concise explanation that gives guarantees about data
transfer; as a precaution, the datapoints encode a schema version in the
`DP_BEGIN_VERSION (0x00)` field and the number of internal fields in the
`DP_END_LENGTH (0xFF)` field.

*I'm not an expert in Bluetooth, so will not get the jargon right. Please
open an issue or PR to correct any terminological impropriety!*

In order to get the BLE server to broadcast notifications, it's necessary to
discover and set the characteristic's CCC attribute to notify:

```bash #Find the bluetooth device address hcitool lescan 

# List all characteristics; here, you can see the service's UUID
gatttool -b <DEVICE ADDRESS> --characteristics 

# List service characteristic values (and their associated handles)
gatttool -b <DEVICE ADDRESS> --char-read-uuid <SERVICE UUID>

# Discover 2902 CCC attributes for a characteristic
gatttool -b <DEVICE ADDRESS> --char-read-uuid 2902 <CHARACTERISTIC HANDLE>

# Write 0100 `enable notifications` to the 2902 CCC attribute for our
# characteristic, and then listen for notifications
gatttool -b <DEVICE ADDRESS> --char-write-req <2902 CCC HANDLE> 0100 --listen
```
