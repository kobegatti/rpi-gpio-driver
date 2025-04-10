# Makefile for building the executable

CC = gcc
CFLAGS = -Wall -Wextra -Wpedantic -g -Werror -std=c99

# Set the default executable name if not provided
EXEC = blinkLED

# List of source files (You can modify this list as needed)
SRC = $(wildcard *.c)
OBJ = $(SRC:.c=.o)

# Default target: build the executable
all: $(EXEC)

# Link the object files to create the executable
$(EXEC): $(OBJ)
	$(CC) $(OBJ) -o $@

# Compile the source files to object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up the build artifacts
clean:
	rm -f $(OBJ) $(EXEC)

run: $(EXEC)
	./(EXEC)

# Phony targets
.PHONY: all clean
