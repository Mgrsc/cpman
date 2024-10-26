# Compiler and flags
CC := gcc
CFLAGS += -Wall -Wextra -std=c11 -O2 -D_GNU_SOURCE -fPIC
LDFLAGS +=

# Directories
SRC_DIR := .
OBJ_DIR := obj
BIN_DIR := bin

# Source files and object files
SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS := $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

# Target executable
TARGET := $(BIN_DIR)/cpman

# Installation directory
INSTALL_DIR ?= /usr/local/bin

# Phony targets
.PHONY: all clean install uninstall debug

# Default target
all: $(TARGET)

# Debug target
debug: CFLAGS += -g -O0
debug: all

# Rule to create object directory
$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

# Rule to create bin directory
$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# Rule to compile source files into object files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Rule to link object files into the target executable
$(TARGET): $(OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(OBJS) -o $(TARGET) $(LDFLAGS)

# Clean rule
clean:
	$(RM) -r $(OBJ_DIR) $(BIN_DIR)

# Install rule
install: $(TARGET)
	install -d $(INSTALL_DIR)
	install -m 755 $(TARGET) $(INSTALL_DIR)

# Uninstall rule
uninstall:
	$(RM) $(INSTALL_DIR)/$(notdir $(TARGET))

# Include dependencies
-include $(OBJS:.o=.d)

# Rule to generate dependency files
$(OBJ_DIR)/%.d: $(SRC_DIR)/%.c | $(OBJ_DIR)
	@set -e; rm -f $@; \
	$(CC) -MM $(CFLAGS) $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,$(OBJ_DIR)/\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

# Optional: reduce output unless V=1 is used
ifndef V
.SILENT:
endif

