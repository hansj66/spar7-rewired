
# Spar 7 Controller

This repository contains schematics, PCB layout, gerber files and STEP/STL files necessary for a complete Spar 7 rebuild.

 > Disclaimer: This repository contains information about my own rebuild. I can not guarantee that this will work as expected in YOUR game. It is entirely possible that you will break your beautiful collector quality game beyond repair trying to retrofit this board and the 3D printed hopper mechanism. Proceed at your own risk :)


## Building the firmware

1. Download [espressif installer ](https://dl.espressif.com/dl/esp-idf/?idf=4.4)
2. Configure environemnt variable IDF_PATH to point to espressif installation folder
3. If using visual studio code: Install the espressif plugin
4. Drop any certs into the certs/ folder. Any files dropped here will be packed into spiffs.bin, which in turn automatically  will be flashed to the spiffs partition. This is only necessary if you want to communicate using dtls over the wifi network.

```
idf.py build 
```

## Flashing

The board can be flashed using a USB Micro B cable. Press the blue button and apply power to the board to enter bootloader mode.

```
idf.py flash -p <port>
```

## Logging / shell

Log output and shell commands are available via USB. Type "Help" to get a list of commands.

```
idf.py monitor
```



## Shell commands

| Command  | Description |
| ------------------------------ | ------------- |
| help  | Print the list of registered commands.  |
| free  | Get the current size of free heap memory.  |
| heap  | Get minimum size of free heap memory that was available during program execution.|
| version  | Get version of chip, SDK and firmware.  |
| restart | Software reset of the chip. |
| tasks  | Get information about running tasks. |
| log_level  \<tag\|\*\> \<none\|error\|warn\|debug\|verbose\>  |  Set log level for all tags or a specific tag. |
| get_delays  | Get information about current debounce / delay settings. |
| set_delay  <hopper\|exit\|payout> \<ms\>  |  Set debounce delay for a switch group (hopper, coin exit or payout) in milliseconds.|
| set_wifi \<ssid\> \<password\>  |  Set wifi ssid/password for your local network. (required for backend   functionality). |
| ip  |  Get IP address. |
| dir  |  Lists files in filesystem. |
| txtest  |  Sends a hello world message to the backend. |


## Necessary hardware changes.

* Remove the internal lighting, power supply, hopper, printed circuit boards and associated wiring.
* 3D print all STL files in the "3D-print\STL" folder and assemble the new hopper (You will also need an assortment of 6mm, 15mm and 20mm M3 screws and 4 wood screws). The hopper is designed for a 12V JBG37-520 geared DC motor (60 or 120 RPM). If a motor with a different form factor is going to be used, you will have to import, modify and reslice the step files accordingly.
* Replace the existing fluorescent tube with a 12V LED strip. This can be powered from the same power supply as the motor.

> Note: The 7 segment display is optional. The board will work fine for games that don't have display option.

### 1. Switch wiring
* Remove (or modify) the existing wiring harness. If the switch terminal lugs are heavily oxidized, it may be a good idea to solder the wiring harness directly to the lugs instead of using connectors.
* Route a common switch ground wire through all payout switches, coin exit switch and hopper switch and connect this to the GND terminal on the 8 pin terminal block.
* Route all wires from the "2"-switches to the ""Pay 2" terminal, "3"-switches to the "Pay 3" etc. Also connect the "coin exit" and hopper switches to the corresponding terminals (The "Ext" terminal is currently not in use)

### 2. Motor wiring
1. Connect a 12V 1A power supply to the power pluggable terminal block.
1.  Connect the motor in series with a separate 12V power supply and connect the remaining wire going from the power supply and the remaning wire going from the motor to the "Motor Relay" pluggable terminal block.
1. In case the default debounce delays don't work for your setup, these delays can be changed using shell commands (the shell is always available via the USB.)

Example:

Changing the hopper debounce delay to 250ms:
```
set_delay hopper 250
```
## IoT Backend Notes

If you are using the [https://span.lab5e.com/](https://span.lab5e.com/) IoT backend, you will have to create a representation of your device and download the necessary certificates. (It's free to register and use for hobbyists level number of devices and data. You'll find all necessary information in the [Getting Started section](https://www.lab5e.com/docs/get_started/) of the documentation).

1. Create a folder under firmware (firmware\certs) and drop the <deviceid>-cert.crt, the <deviceid>-key.key and the span-cert-chain.crt in this folder
1. Concatenate <deviceid>-cert.crt and span-cert-chain.crt to a new 'cert.crt' file in the same folder (the device cert has to be the first certificate).

This folder will automatically be flashed to the spiffs partition when you flash the firmware. If you set the wifi ssid and password using the command shell, the firmware will try to connect to your wifi. If successful, bookkeeping data will then be sent when coins are received or paid out from the game using dtls.

The protocol is rather simple. Messages consist of one byte only. A positive number == coin in. A negative number == coins out. (I considered using protobuffers, but decided it would be overkill for now...)

The Span backend provides data retention. It also has a REST api and various outputs that make it simple to write a small application/backend to retrieve these messages from Span at your convenience. [Example clients](https://www.lab5e.com/docs/backend/) are available for a wide variety of languages.





