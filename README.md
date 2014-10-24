This sketch will connect to api.thingspeak.com and update field1 in a channel with current millis() value

Configuration is done via the serial monitor. To configure, make sure you have the arduino serial monitor set to send CR at the end of each line.
You will then be asked to enter your WIFI SSID, password and thingspeak key
Configuration values are stored in EEPROM

This is currently for Teensy3 only.

Some code based on http://hackaday.io/project/3072/instructions

Connections
===========

 * Wifi RX -> Teensy3 pin 1
 * Wifi TX -> Teensy3 pin 0
 * Wifi RESET -> Teensy3 pin 2
 * Wifi CH PD -> 3.3V

I found that I needed to supply the module from a separate 3.3v supply. The regulator in the teensy cannot provide enough current


