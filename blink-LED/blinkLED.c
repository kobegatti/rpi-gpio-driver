#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define GPIO_MIN 2
#define GPIO_MAX 27
#define VALID_NUM_ARGS 2
#define DEVICE_NAME_LEN 16


// Global variables
static int fd = 1;
static const char* GPIO_DRIVER_DIR = "/dev/led";
static char device_name[DEVICE_NAME_LEN] = "";


/* Function Prototypes */
void catchSignalAndExit(int sig);
void cleanUp();
int closeFile();
int isValidPin(int pin);
void offGPIO(int fd);
void onGPIO(int fd);
int openFile(const char* path, int flags);
void printUsage(char* name);
int strToInt(char* s);

int main(int argc, char* argv[]);


/* Function Definitions */
void catchSignalAndExit(int sig)
{
	int ret = 0;

	printf("Caught signal %d, exiting...\n", sig);

	ret = closeFile();
	printf("closed fd %d and returned %d...\n", fd, ret);

	if (ret != 0) { exit(1); }

	exit(0);
}

int closeFile()
{
	int ret = 0;

	offGPIO(fd);

	ret = close(fd);
	if (ret < 0)
	{
		perror("close");
	}

	return ret;
}

int isValidPin(int pin)
{
	if (pin < GPIO_MIN || pin > GPIO_MAX)
	{
		return 0;
	}

	return 1;
}

void offGPIO(int fd)
{
	write(fd, "0", 1);
	printf("write \"0\" to fd %d\n", fd);
}

void onGPIO(int fd)
{
	write(fd, "1", 1);
	printf("write \"1\" to fd %d\n", fd);
}

int openFile(const char* path, int flags)
{
	int ret = 0;

	ret = open(path, flags);
	if (ret < 0)
	{
		perror(path);
		exit(1);
	}

	printf("opened %s... fd: %d\n", path, fd);

	return ret;
}

void printUsage(char* name)
{
	fprintf(stderr, "Usage: %s <gpio_pin_number>\n", name);

	return;
}

int strToInt(char* s)
{
	int i;
	char* endPtr = NULL;

	i = strtol(s, &endPtr, 10);
	if (*endPtr != '\0')
	{
		fprintf(stderr, 
			"Invalid input(%s): Conversion stopped at \"%s\"\n", s, endPtr);
		exit(1);
	}

	return i;
}

int main(int argc, char* argv[])
{
	if (argc != VALID_NUM_ARGS)
	{
		printUsage(argv[0]);
		exit(1);
	}

	int pin = strToInt(argv[1]);
	if (!isValidPin(pin))
	{
		fprintf(stderr, 
			"Invalid pin %d: %d <= (int)pin <= %d are valid GPIO pins\n", 
			pin, GPIO_MIN, GPIO_MAX);
		exit(1);
	}

	strncpy(device_name, GPIO_DRIVER_DIR, sizeof(device_name));
	size_t len = strlen(device_name);

	snprintf(device_name + len, sizeof(device_name) - len, "%d", pin);
	printf("device_name: %s\n", device_name);

	if (access(device_name, F_OK) != 0)
	{
		fprintf(stderr, "Filepath \"%s\" doesn't exist\n", device_name);
		exit(1);
	}

	fd = openFile(device_name, O_RDWR);

	signal(SIGINT, catchSignalAndExit); // Ctrl+C
	signal(SIGTERM, catchSignalAndExit); // terminate signal
	signal(SIGTSTP, catchSignalAndExit); // Ctrl+Z

	while (1)
	{
		offGPIO(fd);
		sleep(1);
		onGPIO(fd);
		sleep(1);
	}

	return 0;
}
