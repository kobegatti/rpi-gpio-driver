#include <asm/io.h>
#include <linux/device.h>


#define MAX_BUFFER_SIZE 1024

// cat /proc/iomem | grep -i gpio
#define GPIO_BASE_ADDRESS 0x3f200000 // RPi 3 Model B v1.2

const short GPSET0_OFFSET = 0x1c;
const short GPCLR0_OFFSET = 0x28;

const short GPIO_MIN = 2;
const short GPIO_MAX = 27;

static volatile uint32_t* gpio_registers = NULL;

static struct class* my_class = NULL;
static struct device** my_devices = NULL;
static int major = -1;

static const char* device_prefix = "gpio";
static char device_name[8] = {0}; 

// For each device
struct gpio_dev
{
	int gpio_pin;
	char buffer[MAX_BUFFER_SIZE]; 
};

static int gpio_pins[32] = {0};
static int gpio_pins_count = 0;
module_param_array(gpio_pins, int, &gpio_pins_count, 0444);
MODULE_PARM_DESC(gpio_pins, "int array parameter for \"/dev/led<gpio_pin>\" files");

static void gpioSetOutput(unsigned int pin)
{
	printk(KERN_INFO "gpioSetOutput(%d)\n", pin);

	unsigned int fsel_index = pin / 10;
	unsigned int fsel_bitops = pin % 10;

	printk("fsel_index: %u\n", fsel_index);
	printk("fsel_bitops: %u\n", fsel_bitops);

	volatile uint32_t* gpio_fsel = gpio_registers + fsel_index;

	printk(KERN_INFO "gpio_fsel(%p): 0x%X\n", (void*)gpio_fsel, *gpio_fsel);
	*gpio_fsel &= ~(7 << (fsel_bitops * 3)); // clear pin's 3 FSEL bits
	*gpio_fsel |= (1 << (fsel_bitops * 3)); // set LSB of 3 FSEL bits
	printk(KERN_INFO "Updated gpio_fsel(%p): 0x%X\n", (void*)gpio_fsel, *gpio_fsel);
}

static void gpioPinOn(unsigned int pin)
{
	printk(KERN_INFO "gpioPinOn(%d)\n", pin);

	volatile uint32_t* gpio_on_register = (volatile uint32_t*)((char*)gpio_registers + GPSET0_OFFSET);

	*gpio_on_register = (1 << pin);

	return;
}

static void gpioPinOff(unsigned int pin)
{
	printk(KERN_INFO "gpioPinOff(%d)\n", pin);

	volatile uint32_t* gpio_off_register = (volatile uint32_t*)((char*)gpio_registers + GPCLR0_OFFSET);

	*gpio_off_register = (1 << pin);

	return;
}

static int dev_match(struct device* dev, const void* data)
{
	dev_t* devt = (dev_t*)data;

	return dev->devt == *devt;
}

static int dev_open(struct inode* inode, struct file* file)
{
	int minor = iminor(inode);
	if (minor < 0 || minor >= gpio_pins_count)
	{
		printk(KERN_ERR "Invalid minor number: %d\n", minor);
		return -ENODEV;
	}

	dev_t devt = MKDEV(major, minor);
	struct device* dev = class_find_device(my_class, NULL, &devt, dev_match);
	if (NULL == dev) 
	{ 
		printk(KERN_ERR "Device not found for devt %u:%u\n", MAJOR(devt), MINOR(devt));
		return -ENODEV; 
	}

	struct gpio_dev* gdev = dev_get_drvdata(dev);
	if (NULL == gdev) 
	{ 
		printk(KERN_ERR "No drvdata associated with device\n");
		return -EINVAL; 
	}

	file->private_data = gdev;
	printk(KERN_INFO "Device for GPIO pin %d opened\n", gdev->gpio_pin);

	return 0;
}

static int dev_release(struct inode* inode, struct file* file)
{
	struct gpio_dev* gdev = file->private_data;
	if (gdev)
	{
		printk(KERN_INFO "Device for GPIO pin %d released\n", gdev->gpio_pin);
	}
	else
	{
		printk(KERN_INFO "Device with NULL private_data released\n");
	}

	return 0;
}

ssize_t dev_read(struct file* file, char __user* user, size_t size, loff_t *off)
{
	struct gpio_dev* gdev = file->private_data;
	char* buffer = gdev->buffer;
	unsigned long ret = 0;
	size_t len = strlen(buffer);

	// If offset is greater than length of data
	if (*off >= len)
	{
		return 0; // then no more data to read
	}

	// Ensure to not read more data than available
	if (size > (len - *off))
	{
		size = len - *off;
	}

	ret = copy_to_user(user, buffer + *off, size);
	if (ret)
	{
		printk("%lu bytes were not copied to user\n", ret);
		return -EFAULT;
	}

	// update offset
	*off += size;

	printk(KERN_INFO "Read %zu bytes, offset: %lld\n", size, *off);
	return size; 
}

ssize_t dev_write(struct file* file, const char __user* user, size_t size, loff_t* off)
{
	struct gpio_dev* gdev = file->private_data;
	int gpio_pin = gdev->gpio_pin;
	unsigned int value = UINT_MAX;
	unsigned long ret = 0;

	// Clear buffer
	memset(gdev->buffer, 0x0, sizeof(gdev->buffer));

	if (size > MAX_BUFFER_SIZE)
	{
		size = MAX_BUFFER_SIZE;
	}

	ret = copy_from_user(gdev->buffer, user, size);
	if (ret)
	{ 
		printk(KERN_ERR "%lu bytes were not copied from user\n", ret);
		return ret; 
	}


	printk("Buffer: %s\n", gdev->buffer);

	if (sscanf(gdev->buffer, "%d", &value) != 1)
	{
		printk(KERN_ERR "Invalid data format submitted\n");
		return size;	
	}

	if (0 != value && 1 != value)
	{
		printk(KERN_ERR "Invalid off(0)/on(1) value: %d entered\n", value);
		return size;
	}

	if (0 == value)
	{
		gpioPinOff(gpio_pin);
	}
	else if (1 == value)
	{
		gpioPinOn(gpio_pin);
	}

	return size;
}

static struct file_operations fops = 
{
	.owner = THIS_MODULE,
	.read = dev_read,
	.write = dev_write,
	.open = dev_open,
	.release = dev_release,
};


static void releaseResources(void)
{
	printk(KERN_INFO "\nRelease resources\n");

	for (size_t i = 0; i < gpio_pins_count; ++i)
	{
		struct gpio_dev* gdev = dev_get_drvdata(my_devices[i]);
		kfree(gdev);
		device_destroy(my_class, MKDEV(major, i));
	}

	kfree(my_devices);
	class_destroy(my_class);
	unregister_chrdev(major, device_name);

	return;
}

static int isValidGPIO(int pin)
{
	if (pin < GPIO_MIN || pin > GPIO_MAX)
	{
		return 0;
	}

	return 1;
}

static int __init gpio_driver_init(void)
{
	printk(KERN_INFO "\nWelcome to rpi-gpio-driver!\n");

	gpio_registers = (volatile uint32_t*)ioremap(GPIO_BASE_ADDRESS, PAGE_SIZE);
	if (gpio_registers == NULL)
	{
		printk(KERN_ERR "Failed to map GPIO memory to driver\n");
		return -1;
	}

	printk(KERN_INFO "GPIO_BASE_ADDRESS: %#x\n", GPIO_BASE_ADDRESS);
	printk(KERN_INFO "PAGE_SIZE: %lu\n", PAGE_SIZE);

	// Verify GPIO pins entered are valid
	for (size_t i = 0; i < gpio_pins_count; ++i)
	{
		if (!isValidGPIO(gpio_pins[i]))
		{
			printk(KERN_ERR "gpio_pins[%lu]: %d\n", i, gpio_pins[i]);
			printk(KERN_INFO "Valid gpio pins are %d to %d inclusive\n", GPIO_MIN, GPIO_MAX);
			return -1;
		}
	}

	// Register char device
	major = register_chrdev(0, device_name, &fops);
	if (major < 0)
	{
		printk(KERN_ERR "Failed to register char device\n");
		return major;
	}

	// Create device class
	my_class = class_create("my_class");
	if (IS_ERR(my_class))
	{
		unregister_chrdev(major, device_name);
		printk(KERN_ERR "Failed to create device class\n");
		return PTR_ERR(my_class);
	}

	// Allocate memory for devices
	my_devices = kzalloc(sizeof(struct device*) * gpio_pins_count, GFP_KERNEL);
	if (!my_devices)
	{
		class_destroy(my_class);
		unregister_chrdev(major, device_name);
		printk(KERN_ERR "Failed to allocate memory for devices\n");
		return -ENOMEM;

	}
	
	// Create devices for each GPIO pin
	printk(KERN_INFO "%d gpio_pins entered\n", gpio_pins_count);
	for (size_t i = 0; i < gpio_pins_count; ++i)
	{
		// gdev struct to map pin to correct dev file
		struct gpio_dev* gdev = kzalloc(sizeof(struct gpio_dev), GFP_KERNEL);
		if (!gdev) 
		{ 
			printk(KERN_ERR "Failed to create struct gpio_dev*\n");
			return -ENOMEM; 
		}
		gdev->gpio_pin = gpio_pins[i];

		// copy const prefix to device_name
		strncpy(device_name, device_prefix, sizeof(device_name));
		size_t device_name_len = strlen(device_name);
		// append pin number to device_name
		snprintf(device_name + device_name_len, sizeof(device_name) - device_name_len, "%d", gpio_pins[i]);

		printk(KERN_INFO "\ndevice_name: %s\n", device_name);

		// Save gdev in device data array
		my_devices[i] = device_create(my_class, NULL, MKDEV(major, i), gdev, device_name, gpio_pins[i]);
		if (IS_ERR(my_devices[i]))
		{
			printk(KERN_ERR "Failed to create device: %s\n", device_name);
			releaseResources();
			return PTR_ERR(my_devices[i]);
		}

		// Set GPIO pin as output in function select register
		gpioSetOutput(gpio_pins[i]);
	}

	return 0;
}

static void __exit gpio_driver_exit(void)
{
	releaseResources();

	printk("Exiting rpi-gpio-driver!\n");

	return;
}

module_init(gpio_driver_init);
module_exit(gpio_driver_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Low Level Learning");
MODULE_DESCRIPTION("open/close/read/write '0' or '1' to /dev/gpio<pin> files on RPI3");
MODULE_VERSION("1.2");
