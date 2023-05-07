## ESP-Miner

ESP-Miner is bitcoin miner software designed to run on the ESP32. It mines on ASICs such as the Bitmain BM1397. The [Bitaxe](https://github.com/skot/bitaxe/) is a handy board for this!

![esp-miner block diagram](doc/diagram.png)

## Architecture
- Uses the ESP-IDF Framework, which is based on FreeRTOS (no Linux involved)
- Uses the ESP32 WiFi to connect to the network
- The ESP32 is provisioned to the local WiFi over BLE using ESP SmartConfig

### Firmware
- ESP-Miner connects to and verifies the attached mining ASIC.
    - Setting the baud rate, hashing frequency, and filling in some other mystery registers
    - This startup sequence for the BM1387 and BM1397 can be found in cgminer the Kano edition in [driver-gekko.c](https://github.com/kanoi/cgminer/blob/master/driver-gekko.c)

- ESP-Miner connects to your pool or stratum server and subscribes to get the latest work.
    - This is done with [JSON-RPC](https://www.jsonrpc.org)
    - Via the [Stratum](https://braiins.com/stratum-v1/docs) protocol
    - [cJSON](https://github.com/DaveGamble/cJSON) seems like a good embedded library for doing this.
- ESP-Miner gets the latest work and formats it to be sent to the mining ASIC.
    - There isn't much change here except for computing the midstates, and shifting some bytes around
        - Beware of endianess!
        - How do we do this? Examples in cgminer Kano edition.
        - I have started on this.. [check this](nm1397_protocol.md)
- ESP-Miner sends this work to the mining ASIC over serial.

- The Mining ASIC will report back any when it finds a hash over the difficulty (which)?
    - The catch here is that if it doesn't find a hash over the difficulty, it will not report back at all. So you need to keep track of the hashing frequency and the time so that you can send a new block header to be hashed.

- ESP-Miner will report back to the pool over Stratum the results of mining.

- All of the administrative stuff of running a miner like;
     - Set the BM1397 core voltage and current.
     - check the fan speed
     - check the BM1397 temperature
     - optimize the hashing frequency and core voltage for max efficiency

## Hardware Required

This example can be run on any commonly available ESP32 development board.

## Configure the project

Set the target

```
idf.py set-target esp32s3
```

Use menuconfig to set the stratum server address/port and WiFi SSID/Password
```
idf.py menuconfig
```
Set following parameters under Example Configuration Options, these will define the stratum server you connect to:

* Set IP version of example to be IPV4 or IPV6.

* Set IPV4 Address in case your chose IP version IPV4 above.

* Set IPV6 Address in case your chose IP version IPV6 above.

    Set Port number that represents remote port the example will connect to.

Set following parameters under Example Connection Configuration Options:

* Set `WiFi SSID` to your target wifi network SSID.

* Set `Wifi Password` to the password for your target SSID.

For more information about the example_connect() method used here, check out https://github.com/espressif/esp-idf/blob/master/examples/protocols/README.md.

Username is currently hardcoded in miner.c. Modify the define to change.

```
#define STRATUM_USERNAME "johnny9.esp"
```

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

