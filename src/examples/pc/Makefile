#/bin/bash

#TARGET = demo
#TARGET = all
#TARGET = var
#TARGET = play
TARGET = stress
#TARGET = long
#TARGET = float
#TARGET = style

TARGET_DIR = src
STUBS_DIR = src
LOOKER_DIR = ../../looker
LOOKER_WIFI_DIR = ../../looker_wifi
LIBCRC_DIR = ../../crc8
BIN_DIR = bin
CFLAGS = -Wall -O2 -c -MMD -I$(shell pwd)/$(STUBS_DIR)
LDFLAGS =
PORT = /dev/ttyUSB0
VERBOSE = 1
ifeq ($(VERBOSE), 1)
	ECHO =
else
    ECHO = @
endif

OBJ = $(BIN_DIR)/$(TARGET).o $(BIN_DIR)/looker.o $(BIN_DIR)/looker_stubs.o $(BIN_DIR)/crc8.o

.PHONY: all run clean debug

all: $(BIN_DIR) $(BIN_DIR)/$(TARGET)

$(BIN_DIR):
	$(ECHO) mkdir -p $(BIN_DIR)

$(BIN_DIR)/$(TARGET): $(OBJ)
	$(ECHO) gcc -o $@ $^ $(LDFLAGS) 

$(BIN_DIR)/$(TARGET).o: $(TARGET_DIR)/$(TARGET).c
	$(ECHO) gcc $(CFLAGS) -o $@ $<

$(BIN_DIR)/looker.o: $(LOOKER_DIR)/looker.c
	$(ECHO) gcc $(CFLAGS) -o $@ $<

$(BIN_DIR)/looker_stubs.o: $(STUBS_DIR)/looker_stubs.c
	$(ECHO) gcc $(CFLAGS) -o $@ $<

$(BIN_DIR)/crc8.o: $(LIBCRC_DIR)/crc8.c
	$(ECHO) gcc $(CFLAGS) -o $@ $<

run: $(BIN_DIR) $(BIN_DIR)/$(TARGET)
	$(ECHO) $(BIN_DIR)/$(TARGET) $(PORT)

debug: $(BIN_DIR) $(BIN_DIR)/$(TARGET)
	$(ECHO) gdb -tui --args $(BIN_DIR)/$(TARGET) $(PORT)

clean:
	$(ECHO) rm -rf $(BIN_DIR)

-include $(OBJ:.o=.d)

