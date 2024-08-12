# Keithley Data Collection
A small Arduino program to fetch readings from a Keithley 2410 SourceMeter. The SourceMeter's RS232 port is connected by a null modem RS232 cable (which switches RX/TX) through a Max3232 chip (to convert the RS232 voltage levels at ±15 V to 5V TTL) to the arduino.
