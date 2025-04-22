# rpi-gpio-driver
Raspberry Pi 3

opens/closes /dev files
reads/writes "1" and "0" to GPIO pins 2 to 27.

e.g.
	make
	sudo insmod rpi-gpio-driver.ko gpio_pins=5,13,16,17,22
	sudo rmmod rpi-gpio-driver
	make clean
