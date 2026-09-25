# Miata ND3 Tach Ring LED

Using an Adafruit Feather M4 CAN Express to power a ring of 22x WS2812B 5050 Narrow NeoPixels arranged around the ND3 Tachometer.

Interfacing with the OBD-II port to get Engine RPM and the estimated Oil Temp.
Goal is to have the same functionality of the upstream repo with the LED ring illuminating a faux redline, as well as a flashing shift indicator at optimal shift RPM ~7000-75000.

Currently able to get a 20Hz update on the RPM since the ND3 CAN isn't broadcasting constantly and everything needs to be requested manually.
Thanks to https://github.com/drewid74/2024-nd3-mazda-obdii for the updated ND3 CAN research.

TODO
-Write code for LED animations.
-Install an Oil temp sensor to get real data rather than the estimated calculation from the CAN.
-Find a way to get dash brightness. Maybe it is available via CAN since it also affects infotainment brightness.