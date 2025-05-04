# rpi-gpio-driver
Raspberry Pi 3

opens/closes /dev files for GPIO pins 2 to 27.
reads/writes "0" and "1" to /dev files.

e.g.
	make
	sudo insmod rpi-gpio-driver.ko gpio_pins=5,13,16,17,22
	sudo rmmod rpi-gpio-driver
	make clean
