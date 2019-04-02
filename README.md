# SimpleCommand
The nRF52 companion to SimpleBLECommander

## Requirements
This example was build using the Nordic 15.2 SDK.

It is intended to be run as a Makefile project.

The current target supported are the nRF52840-DK and the nRF52840 Dongle (pca10056 and pca10059, respectively).

## Building

Clone the project into `<path to>/<nrf52 15.2 sdk directory>/applications/SimpleCommand/` where, if need be, you have created an `applications` folder at the root of the SDK folder (i.e, at the sample level as `examples`)

```
$cd <path to>/<nrf52 15.2 sdk directory>/applications/SimpleCommand/simpleCommand
$cd pca1005x/s140/argmcc/ # either pca10040, pca10056, or pca10059, depending on your target
$make clean # just to be sure
$make
```

At that point you can use your preferred method to flash. If you are using the Dongle, then an extra makefile command is availble to create a hex image

`$make hex`

For the Dongle, I recommend using [nRF Connect](https://www.nordicsemi.com/Software-and-Tools/Development-Tools/nRF-Connect-for-desktop) to program it with the resulting hex file. 

The result can be tested using the iOS app from [knud/SimpleBLECommander](https://github.com/knud/SimpleBLECommander)
## Issues

Please post them to the repo.
