| Supported Targets | ESP32-S3 |
| ----------------- | -------- |


# ESP-miner

## Requires ESP-IDF v4.4.4

## Hardware Required

This firmware is designed to run on a BitAxe v2+

## Configure the project

Set the target

```
idf.py set-target esp32s3
```

Use menuconfig to set the stratum server address/port and WiFi SSID/Password
```
idf.py menuconfig
```
Set following parameters under Stratum Configuration Options, these will define the stratum server you connect to:

* Set `Stratum Address` to the stratum pool domain name. example "solo.ckpool.org"

* Set `Stratum Port` to the stratum pool port. example "3333"

* Set `Stratum username` to the stratum pool username

* Set `Stratum password` to the stratum pool password

Set following parameters under Example Connection Configuration Options:

* Set `WiFi SSID` to your target wifi network SSID.

* Set `Wifi Password` to the password for your target SSID.


For more information about the example_connect() method used here, check out https://github.com/espressif/esp-idf/blob/master/examples/protocols/README.md.


## Build and Flash

Build the project and flash it to the board, then run monitor tool to view serial output:

```
idf.py -p PORT flash monitor
```

(To exit the serial monitor, type ``Ctrl-]``.)

See the Getting Started Guide for full steps to configure and use ESP-IDF to build projects.

## Run Unit tests

The unit tests for the project use the unity test framework and currently require actual esp32 hardware to run.

They are located at https://github.com/johnny9/esp-miner/tree/master/components/stratum/test

```
cd ./test/
idf.py set-target esp32s3
idf.py -p PORT flash monitor
```

