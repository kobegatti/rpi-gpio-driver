#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define GPIO_MIN 2
#define GPIO_MAX 27
#define MIN_NUM_ARGS 2
#define DEVICE_NAME_LEN 16


// Global variables
const char* DEV_PREFIX = "/dev/led";
static char device_name[DEVICE_NAME_LEN] = "";
static int* device_fds = NULL;
static int num_fds = 0;


// Function Protoypes
void catchSignalAndExit(int sig);
void cleanUp();
int isValidGPIOPin(int pin);
void ledRoutine1();
void OffGPIO(int fd);
void OnGPIO(int fd);
int openFile(const char* path, int flags);
void printUsage(char* program_name);
int strToInt(char* s);

int main(int argc, char* argv[]);


// Function Definitions
void catchSignalAndExit(int sig)
{
	printf("Caught signal %d, cleaning up...\n", sig);

	for (int i = 0; i < num_fds; ++i)
	{
		OffGPIO(device_fds[i]);
	}

	cleanUp();

	exit(0);
}

void cleanUp()
{
	int ret = 0;

	for (int i = 0; i < num_fds; ++i)
	{
		ret = close(device_fds[i]);
		if (ret < 0)
		{
			perror("close");
		}
		else
		{
			printf("closed fd %d and returned %d...\n", device_fds[i], ret);
		}
	}

	free(device_fds);
	device_fds = NULL;
}

int isValidGPIOPin(int pin)
{
	if (pin < GPIO_MIN || pin > GPIO_MAX)
	{
		return 0;
	}

	return 1;
}

void ledRoutine1()
{
	for (int i = 0; i < num_fds; ++i)
	{
		OnGPIO(device_fds[i]);
		sleep(1);
	}

	for (int i = 0; i < num_fds; ++i)
	{
		OffGPIO(device_fds[i]);
		sleep(1);
	}

	for (int i = num_fds - 1; i >= 0; --i)
	{
		OnGPIO(device_fds[i]);
		sleep(1);
	}

	for (int i = num_fds - 1; i >= 0; --i)
	{
		OffGPIO(device_fds[i]);
		sleep(1);
	}
}

void OffGPIO(int fd)
{
	write(fd, "0", 1);
	printf("write \"0\" to fd %d\n", fd);
}

void OnGPIO(int fd)
{
	write(fd, "1", 1);
	printf("write \"1\" to fd %d\n", fd);
}

int openFile(const char* path, int flags)
{
	int fd = 0;

	fd = open(path, flags);
	if (fd < 0)
	{
		perror(path);
		exit(1);
	}

	printf("opened %s... fd: %d\n", path, fd);

	return fd;
}

void printUsage(char* program_name)
{
	fprintf(stderr, "Usage: %s <gpio_pin1> <gpio_pin2> ... <gpio_pinN>\n", program_name);
	fprintf(stderr, "Example: %s 17 22 23 24 27\n", program_name);
	fprintf(stderr, "2 <= (int)gpio_pin <= 27\n");

	return;
}

int strToInt(char* s)
{
	int i = 0;
	char* endPtr = NULL;

	i = strtol(s, &endPtr, 10);
	if (*endPtr != '\0')
	{
		fprintf(stderr, 
			"Invalid input (%s): Conversion stopped at \"%s\"\n", s, endPtr);
		exit(1);
	}

	return i;
}

int main(int argc, char* argv[])
{
	// Must enter at least 1 GPIO pin number
	if (argc < MIN_NUM_ARGS)
	{
		printUsage(argv[0]);
		exit(1);
	}
	
	// Verify that each pin number input is a valid GPIO pin
	for (int i = 1; i < argc; ++i)
	{
		int gpio_pin = strToInt(argv[i]);
		if (!isValidGPIOPin(gpio_pin))
		{
			printUsage(argv[0]);
			exit(1);
		}
	}

	num_fds = argc - 1;
	device_fds = malloc(sizeof(int) * num_fds);

	for (int i = 1; i <= num_fds; ++i)
	{
		// copy const prefix to device_name
		strncpy(device_name, DEV_PREFIX, sizeof(device_name));

		size_t prefix_len = strlen(device_name);
		int gpio_pin = strToInt(argv[i]);

		// append GPIO pin num to device_name
		snprintf(device_name + prefix_len, sizeof(device_name) - prefix_len, "%d", gpio_pin);

		if (access(device_name, F_OK) != 0)
		{
			fprintf(stderr, "Filepath \"%s\" doesn't exist\n", device_name);
			cleanUp();
			exit(1);
		}

		printf("device_name: %s\n", device_name);
		device_fds[i - 1] = openFile(device_name, O_RDWR);
	}

	signal(SIGINT, catchSignalAndExit); // Ctrl+C
	signal(SIGTERM, catchSignalAndExit); // terminate signal
	signal(SIGTSTP, catchSignalAndExit); // Ctrl+Z

	while (1)
	{
		ledRoutine1();
	}

	cleanUp();

	return 0;
}
