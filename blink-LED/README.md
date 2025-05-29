Program that toggles a GPIO pin on a Raspberry Pi 3 Model B v1.2
Prerequisites:
	- build rpi-gpio-driver project
	- sudo insmod rpi-gpio-driver.ko gpio_pins=<2>,...,<N> (2 <= N <= 27)
		e.g. sudo insmod rpi-gpio-driver.ko gpio_pins=5,8
	- sudo rmmod rpi_gpio_driver
