# waterheater
A high-tech, low-tech wood fired water heater monitor.  High-tech because 
it's a thing on the Internet of Things (tm).  Low-tech because it works for
a redneck like me.

This arduino project features analog reading of two 10k thermistor sensors
(one top and one bottom) to read the water temperature, and a 30mV 
thermocouple in the stack to determine fire status.

The doublebarrel branch is for my new outdoor wood boiler that utilizes two fuel oil tanks as the water jackets.  These are mounted side-by-side with a space between to build a fire.  This branch's code has enhancements for a HD44780 display, draft control, and an instrumentation amp setup for better reading of the negative voltage that comes out of a 30mV thermocouple.
